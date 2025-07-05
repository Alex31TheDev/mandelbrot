#pragma once

#include <cstdint>
#include <string>
#include <chrono>
#include <tuple>
#include <atomic>
#include <mutex>
#include <variant>

#include "cqueue.h"

struct ProgressConfig {
    bool formatTime = true;
    const std::string progressName = "Progress";
    const std::string opsName = "ops";
};

class ProgressTracker {
public:
    typedef std::chrono::steady_clock::time_point STP;
    typedef std::chrono::milliseconds SU;
    static constexpr double SHORT_UNIT_SCALE = 1.0e3;

    explicit ProgressTracker(int totalWork, bool threadSafe,
        const ProgressConfig &config = ProgressConfig());

    [[nodiscard]] bool completed() const { return _completed; }

    [[nodiscard]] STP startTime() const { return _startTime; };
    [[nodiscard]] int64_t startTimeEpoch() const;

    [[nodiscard]] SU elapsed() const;
    [[nodiscard]] int64_t elapsedInt() const {
        return static_cast<int64_t>(elapsed().count());
    };

    [[nodiscard]] int totalWork() const { return _totalWork; }
    [[nodiscard]] int completedWork() const;

    [[nodiscard]] double percentage() const;
    [[nodiscard]] double opsPerSecond() const;

    void update(int processed = 1, bool printUpdate = true);
    void complete(bool printUpdate = true);

private:
    typedef std::chrono::high_resolution_clock::time_point HTP;
    typedef std::chrono::nanoseconds HU;
    static constexpr double HIGH_UNIT_SCALE = 1.0e9;

    static constexpr int OPS_WINDOW_MIN_SIZE = 10;
    static constexpr int OPS_WINDOW_MAX_SIZE = 200;

    static constexpr int SHORT_TIME_REMAINING = 30;

    int _totalWork;
    bool _threadSafe;
    ProgressConfig _config;

    bool _completed = false;
    STP _startTime, _endTime = STP::min();

    struct _PlainState {
        int completedWork = 0;
        int lastPrinted = -1;

        HTP lastUpdateTime;
    };
    struct _AtomicState {
        std::atomic_int completedWork{ 0 };
        std::atomic_int lastPrinted{ -1 };

        std::atomic<HTP> lastUpdateTime;

        mutable std::mutex opsMutex;
        mutable std::mutex printfMutex;
    };
    std::variant<_PlainState, _AtomicState> _state;

    struct _OpsEntry {
        int count;
        HU time;
    };
    cqueue<_OpsEntry> _opsHistory;

    std::tuple<int, int> _updateWork(int processed);
    void _updateOpsHistory(int processed);

    void _printProgress(int perc);
    void _printElapsed(SU time);
};
