#include "RenderProgress.h"

#include <cstdio>
#include <cstdint>
#include <cinttypes>

#include <atomic>
#include <mutex>
#include <chrono>
#include <type_traits>
#include <variant>

#include "../util/TimeUtil.h"

RenderProgress::RenderProgress(
    int totalWork, bool threadSafe,
    bool formatTime
)
    : _totalWork(totalWork), _threadSafe(threadSafe),
    _formatTime(formatTime),
    _startTime(std::chrono::steady_clock::now()) {
    if (threadSafe) _state.emplace<_AtomicState>();
    else _state.emplace<_PlainState>();

    _printProgress(0);
}

void RenderProgress::_printProgress(int perc) {
    auto print = [&]() {
        printf("\rRendering: %3d%%", perc);
        fflush(stdout);
        };

    std::visit(
        [&](auto &&state) {
            using T = std::decay_t<decltype(state)>;

            if constexpr (std::is_same_v<T, _PlainState>) {
                print();
                state.lastPrinted = perc;
            } else if constexpr (std::is_same_v<T, _AtomicState>) {
                std::scoped_lock lock(_printfMutex);

                print();
                state.lastPrinted.store(perc, std::memory_order_relaxed);
            }
        }, _state
    );
}

void RenderProgress::update(int processed) {
    int current = 0, last = 0;

    std::visit(
        [&](auto &&state) {
            using T = std::decay_t<decltype(state)>;

            if constexpr (std::is_same_v<T, _PlainState>) {
                current = (state.completedWork += processed);
                last = state.lastPrinted;
            } else if constexpr (std::is_same_v<T, _AtomicState>) {
                current = state.completedWork.fetch_add(processed,
                    std::memory_order_relaxed);
                last = state.lastPrinted.load(std::memory_order_relaxed);
            }
        }, _state
    );

    const int perc = (current * 100) / _totalWork;
    if (perc > last) _printProgress(perc);
}

template <typename T>
void RenderProgress::_printElapsed(T elapsed) {
    auto print = [&] {
        if (_formatTime) {
            printf(" (completed in: %s)\n",
                TimeUtil::formatTime(elapsed).c_str());
        } else {
            if constexpr (std::is_same_v<T, int32_t>) {
                printf(" (completed in: %" PRId32 " ms)\n", elapsed);
            } else if constexpr (std::is_same_v<T, int64_t>) {
                printf(" (completed in: %" PRId64 " ms)\n", elapsed);
            }
        }

        fflush(stdout);
        };

    std::visit(
        [&](auto &&state) {
            using T = std::decay_t<decltype(state)>;

            if constexpr (std::is_same_v<T, _PlainState>) {
                print();
            } else if constexpr (std::is_same_v<T, _AtomicState>) {
                std::scoped_lock lock(_printfMutex);
                print();
            }
        }, _state
    );
}

template void RenderProgress::_printElapsed<int32_t>(int32_t);
template void RenderProgress::_printElapsed<int64_t>(int64_t);

void RenderProgress::complete() {
    const auto endTime = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast
        <std::chrono::milliseconds>(endTime - _startTime).count();

    _printProgress(100);
    _printElapsed(elapsed);
}