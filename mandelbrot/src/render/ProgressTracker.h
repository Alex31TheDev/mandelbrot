#pragma once

#include <cstdint>
#include <chrono>
#include <tuple>
#include <atomic>
#include <mutex>
#include <variant>

#include "datastructures/cqueue.h"

#include "BackendAPI.h"

struct ProgressOptions {
    const Backend::Callbacks *callbacks = nullptr;
};

class ProgressTracker {
public:
    typedef uint64_t WU;

    typedef std::chrono::steady_clock::time_point STP;
    typedef std::chrono::milliseconds SU;
    static constexpr double SHORT_UNIT_SCALE = 1.0e3;

    explicit ProgressTracker(WU totalWork, bool threadSafe,
        const ProgressOptions &options = ProgressOptions());

    [[nodiscard]] bool completed() const { return _completed; }

    [[nodiscard]] STP startTime() const { return _startTime; };
    [[nodiscard]] int64_t startTimeEpoch() const;

    [[nodiscard]] SU elapsed() const;
    [[nodiscard]] int64_t elapsedInt() const {
        return static_cast<int64_t>(elapsed().count());
    };

    [[nodiscard]] WU totalWork() const { return _totalWork; }
    [[nodiscard]] WU completedWork() const;

    [[nodiscard]] double percentage() const;
    [[nodiscard]] double opsPerSecond() const;

    void update(WU processed = 1, bool emitUpdate = true);
    void complete(bool emitUpdate = true);

private:
    typedef std::chrono::high_resolution_clock::time_point HTP;
    typedef std::chrono::nanoseconds HU;
    static constexpr double HIGH_UNIT_SCALE = 1.0e9;

    static constexpr int OPS_WINDOW_MIN_SIZE = 10;
    static constexpr int OPS_WINDOW_MAX_SIZE = 200;

    WU _totalWork;
    bool _threadSafe;
    ProgressOptions _options;

    bool _completed = false;
    STP _startTime, _endTime = STP::min();

    struct _PlainState {
        WU completedWork = 0;
        int lastEmitted = -1;

        HTP lastUpdateTime;
    };
    struct _AtomicState {
        std::atomic<WU> completedWork{ 0 };
        std::atomic_int lastEmitted{ -1 };
        std::atomic<HTP> lastUpdateTime{};

        mutable std::mutex opsMutex;
        mutable std::mutex emitMutex;
    };
    std::variant<_PlainState, _AtomicState> _state;

    struct _OpsEntry {
        WU count;
        HU time;
    };
    cqueue<_OpsEntry> _opsHistory;

    std::tuple<int, int> _updateWork(WU processed);
    void _updateOpsHistory(WU processed);

    void _emitProgress(int perc, bool completed);
};
