#include "ProgressTracker.h"

#include <cstdio>
#include <cstdint>
#include <cinttypes>

#include <numeric>
#include <chrono>
#include <tuple>
#include <atomic>
#include <mutex>
#include <variant>
#include <type_traits>

#include "../util/FormatUtil.h"

using namespace std::chrono;

#define CLEAR_LINE "\r\x1b[K"

ProgressTracker::ProgressTracker(
    int totalWork, bool threadSafe,
    const ProgressConfig &config
)
    : _totalWork(totalWork), _threadSafe(threadSafe),
    _config(config),
    _startTime(steady_clock::now()),
    _opsHistory(OPS_WINDOW_MAX_SIZE) {
    if (_threadSafe) _state.emplace<_AtomicState>();
    else _state.emplace<_PlainState>();

    _printProgress(0);
}

int64_t ProgressTracker::startTimeEpoch() const {
    const auto time = duration_cast<SU>(_startTime.time_since_epoch());
    return static_cast<int64_t>(time.count());
}

ProgressTracker::SU ProgressTracker::elapsed() const {
    if (_completed) {
        return duration_cast<SU>(_endTime - _startTime);
    } else {
        const STP now = steady_clock::now();
        return duration_cast<SU>(now - _startTime);
    }
}

int ProgressTracker::completedWork() const {
    if (_completed) return _totalWork;

    return std::visit(
        [&](auto &&state) -> int {
            using T = std::decay_t<decltype(state)>;

            if constexpr (std::is_same_v<T, _PlainState>) {
                return state.completedWork;
            } else if constexpr (std::is_same_v<T, _AtomicState>) {
                return state.completedWork.load(std::memory_order_relaxed);
            } else return 0;
        }, _state
    );
}

double ProgressTracker::percentage() const {
    return (completedWork() * 100) / static_cast<double>(_totalWork);
}

double ProgressTracker::opsPerSecond() const {
    if (_completed) {
        const double timeSec = static_cast<double>(elapsed().count())
            / SHORT_UNIT_SCALE;
        return static_cast<double>(_totalWork) / timeSec;
    }

    auto calc = [](const auto &history) -> double {
        if (history.size() < OPS_WINDOW_MIN_SIZE) return 0.0;

        const int totalOps = std::accumulate(history.begin(), history.end(), 0,
            [](int sum, const _OpsEntry &entry) { return sum + entry.count; });

        const HU totalTime = std::accumulate(history.begin(), history.end(), HU(0),
            [](HU sum, const _OpsEntry &entry) { return sum + entry.time; });

        if (totalTime.count() == 0) return 0.0;
        else {
            const double timeSec = static_cast<double>
                (totalTime.count()) / HIGH_UNIT_SCALE;
            return static_cast<double>(totalOps) / timeSec;
        }
        };

    return std::visit(
        [&](auto &&state) -> double {
            using T = std::decay_t<decltype(state)>;

            if constexpr (std::is_same_v<T, _PlainState>) {
                return calc(_opsHistory);
            } else if constexpr (std::is_same_v<T, _AtomicState>) {
                std::scoped_lock lock(state.opsMutex);
                return calc(_opsHistory);
            } else return 0.0;
        }, _state
    );
}

void ProgressTracker::_printProgress(int perc) {
    auto print = [&]() {
        printf(CLEAR_LINE "%s: %3d%% | %.2f %s/s",
            _config.progressName.c_str(),
            perc, opsPerSecond(),
            _config.opsName.c_str());

        fflush(stdout);
        };

    std::visit(
        [&](auto &&state) {
            using T = std::decay_t<decltype(state)>;

            if constexpr (std::is_same_v<T, _PlainState>) {
                print();
                state.lastPrinted = perc;
            } else if constexpr (std::is_same_v<T, _AtomicState>) {
                {
                    std::scoped_lock lock(state.printfMutex);
                    print();
                }
                state.lastPrinted.store(perc, std::memory_order_relaxed);
            }
        }, _state
    );
}

std::tuple<int, int> ProgressTracker::_updateWork(int processed) {
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
    return std::make_tuple(perc, last);
}

void ProgressTracker::_updateOpsHistory(int processed) {
    auto update = [&](auto &history, HU elapsed) {
        if (history.size() >= OPS_WINDOW_MAX_SIZE) history.pop();
        history.push({ processed, elapsed });
        };

    const HTP now = high_resolution_clock::now();

    std::visit(
        [&](auto &&state) {
            using T = std::decay_t<decltype(state)>;

            if constexpr (std::is_same_v<T, _PlainState>) {
                const auto elapsed = duration_cast<HU>
                    (now - state.lastUpdateTime);
                state.lastUpdateTime = now;

                update(_opsHistory, elapsed);
            } else if constexpr (std::is_same_v<T, _AtomicState>) {
                const HTP last = state.lastUpdateTime
                    .load(std::memory_order_relaxed);
                const auto elapsed = duration_cast<HU>(now - last);
                state.lastUpdateTime.store(now, std::memory_order_relaxed);

                {
                    std::scoped_lock lock(state.opsMutex);
                    update(_opsHistory, elapsed);
                }
            }
        }, _state
    );
}

void ProgressTracker::update(int processed, bool printUpdate) {
    const auto [perc, last] = _updateWork(processed);
    _updateOpsHistory(processed);

    if (printUpdate && perc > last) {
        _printProgress(perc);
    }
}

void ProgressTracker::_printElapsed(SU time) {
    auto print = [&] {
        if (_config.formatTime) {
            const auto elapsed = FormatUtil::formatDuration(time.count());
            printf(" (completed in: %s)\n", elapsed.c_str());
        } else {
            const auto elapsed = time.count();
            using T = std::decay_t<decltype(elapsed)>;

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
                std::scoped_lock lock(state.printfMutex);
                print();
            }
        }, _state
    );
}

void ProgressTracker::complete(bool printUpdate) {
    _completed = true;
    _endTime = steady_clock::now();
    _opsHistory.clear();

    if (printUpdate) {
        _printProgress(100);
        _printElapsed(elapsed());
    }
}