#pragma once

#include <chrono>
#include <atomic>
#include <mutex>

class RenderProgress {
public:
    RenderProgress(int totalRows);
    void update(int processed = 1);
    void complete(bool formatTime = false);

private:
    int _totalRows;
    std::chrono::time_point<std::chrono::steady_clock> _startTime;

    std::atomic<int> _completedRows{ 0 };
    std::atomic<int> _lastPrinted{ -1 };

    std::mutex _printfMutex;

    void _printProgress(int perc);
    template <typename T>
    void _printElapsed(T elapsed, bool format);
};