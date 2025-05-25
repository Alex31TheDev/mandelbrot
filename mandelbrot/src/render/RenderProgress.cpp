#include "RenderProgress.h"

#include <cstdio>
#include <atomic>
#include <mutex>
#include <chrono>

RenderProgress::RenderProgress(int total_rows)
    : _totalRows(total_rows),
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

    int current = _completedRows.load(std::memory_order_relaxed);
    int perc = (current * 100) / _totalRows;
    int last = _lastPrinted.load(std::memory_order_relaxed);

    if (perc > last) {
        _printProgress(perc);
    }
}

void RenderProgress::complete() {
    auto endTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - _startTime).count();

    _printProgress(100);
    printf(" (completed in: %lld ms)\n", elapsed);
}