#include "RenderProgress.h"

#include <cstdio>
#include <cstdint>
#include <cinttypes>

#include <type_traits>
#include <atomic>
#include <mutex>
#include <chrono>

#include "../util/TimeUtil.h"

RenderProgress::RenderProgress(int totalRows)
    : _totalRows(totalRows),
    _startTime(std::chrono::steady_clock::now()) {
    _printProgress(0);
}

void RenderProgress::_printProgress(int perc) {
    std::lock_guard<std::mutex> lock(_printfMutex);

    printf("\rRendering: %3d%%", perc);
    fflush(stdout);

    _lastPrinted.store(perc, std::memory_order_relaxed);
}

void RenderProgress::update(int processed) {
    _completedRows.fetch_add(processed, std::memory_order_relaxed);

    const int current = _completedRows.load(std::memory_order_relaxed);
    const int perc = (current * 100) / _totalRows;
    const int last = _lastPrinted.load(std::memory_order_relaxed);

    if (perc > last) {
        _printProgress(perc);
    }
}

template <typename T>
void RenderProgress::_printElapsed(T elapsed, bool format) {
    std::lock_guard<std::mutex> lock(_printfMutex);

    if (format) {
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
}

template void RenderProgress::_printElapsed<int32_t>(int32_t, bool);
template void RenderProgress::_printElapsed<int64_t>(int64_t, bool);

void RenderProgress::complete(bool formatTime) {
    const auto endTime = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast
        <std::chrono::milliseconds>(endTime - _startTime).count();

    _printProgress(100);
    _printElapsed(elapsed, formatTime);
}