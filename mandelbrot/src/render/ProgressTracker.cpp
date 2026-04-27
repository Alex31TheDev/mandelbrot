#include "ProgressTracker.h"

#include <cstdint>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <tuple>
#include <atomic>
#include <variant>
#include <type_traits>

#include "BackendAPI.h"

using namespace std::chrono;

ProgressTracker::ProgressTracker(
    WU totalWork, bool threadSafe,
    const ProgressOptions &options
)
    : _totalWork(totalWork), _threadSafe(threadSafe),
    _options(options),
    _startTime(steady_clock::now()),
    _opsHistory(OPS_WINDOW_MAX_SIZE) {
    if (_threadSafe) _state.emplace<_AtomicState>();
    else _state.emplace<_PlainState>();

    const HTP now = high_resolution_clock::now();
    std::visit([&](auto &&state) {
        using T = std::decay_t<decltype(state)>;
        if constexpr (std::is_same_v<T, _PlainState>) {
            state.lastUpdateTime = now;
        } else if constexpr (std::is_same_v<T, _AtomicState>) {
            state.lastUpdateTime.store(now, std::memory_order_relaxed);
        }
        }, _state);

    _emitProgress(0, false);
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

ProgressTracker::WU ProgressTracker::completedWork() const {
    if (_completed) return _totalWork;

    return std::visit(
        [&](auto &&state) -> WU {
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
    return (completedWork() * WU(100)) / static_cast<double>(_totalWork);
}

double ProgressTracker::opsPerSecond() const {
    if (_completed || _threadSafe) {
        const double timeSec = static_cast<double>(elapsed().count())
            / SHORT_UNIT_SCALE;
        if (timeSec <= 0.0) return 0.0;

        const double work = _completed
            ? static_cast<double>(_totalWork)
            : static_cast<double>(completedWork());
        return work / timeSec;
    }

    auto calc = [](const auto &history) -> double {
        if (history.size() < OPS_WINDOW_MIN_SIZE) return 0.0;

        const WU totalOps = std::accumulate(history.begin(), history.end(), WU(0),
            [](WU sum, const _OpsEntry &entry) { return sum + entry.count; });

        const HU totalTime = std::accumulate(history.begin(), history.end(), HU(0),
            [](HU sum, const _OpsEntry &entry) { return sum + entry.time; });

        if (totalTime.count() == 0) return 0.0;
        else {
            const double timeSec = static_cast<double>
                (totalTime.count()) / HIGH_UNIT_SCALE;
            return static_cast<double>(totalOps) / timeSec;
        }
        };

    return calc(_opsHistory);
}

void ProgressTracker::_emitProgress(int perc, bool completed) {
    std::visit(
        [&](auto &&state) {
            using T = std::decay_t<decltype(state)>;

            if constexpr (std::is_same_v<T, _PlainState>) {
                const auto *callbacks = _options.callbacks;
                if (callbacks && callbacks->onProgress) {
                    const Backend::ProgressEvent event = {
                        .percentage = perc,
                        .opsPerSecond = opsPerSecond(),
                        .elapsedMs = elapsedInt(),
                        .completedWork = completedWork(),
                        .totalWork = _totalWork,
                        .completed = completed
                    };

                    callbacks->onProgress(event);
                }

                state.lastEmitted = perc;
            } else if constexpr (std::is_same_v<T, _AtomicState>) {
                std::scoped_lock lock(state.emitMutex);
                const int last = state.lastEmitted.load(std::memory_order_relaxed);

                if (!completed && perc <= last) {
                    return;
                }

                const auto *callbacks = _options.callbacks;
                if (callbacks && callbacks->onProgress) {
                    const Backend::ProgressEvent event = {
                        .percentage = perc,
                        .opsPerSecond = opsPerSecond(),
                        .elapsedMs = elapsedInt(),
                        .completedWork = completedWork(),
                        .totalWork = _totalWork,
                        .completed = completed
                    };

                    callbacks->onProgress(event);
                }

                state.lastEmitted.store(perc, std::memory_order_relaxed);
            }
        }, _state
    );
}

std::tuple<int, int> ProgressTracker::_updateWork(WU processed) {
    WU current = 0;
    int lastEmitted = 0;

    std::visit(
        [&](auto &&state) {
            using T = std::decay_t<decltype(state)>;

            if constexpr (std::is_same_v<T, _PlainState>) {
                current = (state.completedWork += processed);
                lastEmitted = state.lastEmitted;
            } else if constexpr (std::is_same_v<T, _AtomicState>) {
                current = state.completedWork.fetch_add(processed,
                    std::memory_order_relaxed) + processed;
                lastEmitted = state.lastEmitted.load(std::memory_order_relaxed);
            }
        }, _state
    );

    current = std::min(current, _totalWork);

    const int perc = _totalWork == 0 ? 100 :
        static_cast<int>((current * WU(100)) / _totalWork);
    return std::make_tuple(perc, lastEmitted);
}

void ProgressTracker::_updateOpsHistory(WU processed) {
    if (_threadSafe) return;

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

void ProgressTracker::update(WU processed, bool emitUpdate) {
    const auto [perc, last] = _updateWork(processed);
    _updateOpsHistory(processed);

    if (emitUpdate && perc > last) {
        _emitProgress(perc, false);
    }
}

void ProgressTracker::complete(bool emitUpdate) {
    _completed = true;
    _endTime = steady_clock::now();

    std::visit(
        [&](auto &&state) {
            using T = std::decay_t<decltype(state)>;
            if constexpr (std::is_same_v<T, _AtomicState>) {
                std::scoped_lock lock(state.opsMutex);
                _opsHistory.clear();
            } else {
                _opsHistory.clear();
            }
        }, _state
    );

    if (emitUpdate) {
        _emitProgress(100, true);
    }
}
