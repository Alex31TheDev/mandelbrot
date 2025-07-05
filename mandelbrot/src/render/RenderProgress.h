#pragma once

#include <chrono>
#include <atomic>
#include <mutex>
#include <variant>

class RenderProgress {
public:
    explicit RenderProgress(int totalWork, bool threadSafe,
        bool formatTime = true);

    void update(int processed = 1);
    void complete();

private:
    int _totalWork;
    bool _threadSafe, _formatTime;

    std::chrono::time_point<std::chrono::steady_clock> _startTime;

    struct _PlainState {
        int completedWork = 0;
        int lastPrinted = -1;
    };
    struct _AtomicState {
        std::atomic<int> completedWork{ 0 };
        std::atomic<int> lastPrinted{ -1 };
    };

    std::variant<_PlainState, _AtomicState> _state;

    std::mutex _printfMutex;

    void _printProgress(int perc);
    template <typename T>
    void _printElapsed(T elapsed);
};
