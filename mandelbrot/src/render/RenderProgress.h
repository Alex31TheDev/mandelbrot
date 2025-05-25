#pragma once

#include <chrono>
#include <atomic>
#include <mutex>

class RenderProgress {
public:
    RenderProgress(int total_rows);
    void update(int rows_processed = 1);
    void complete();

private:
    std::chrono::time_point<std::chrono::steady_clock> _startTime;

    std::atomic<int> _completedRows{ 0 };
    std::atomic<int> _lastPrinted{ -1 };
    std::mutex _printfMutex;
    int _totalRows;

    void _printProgress(int perc);
};