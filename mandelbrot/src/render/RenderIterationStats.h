#pragma once

#include <cstdint>
#include <functional>
#include <optional>

struct RenderIterationStats {
    bool valid = false;
    int64_t min = 0;
    int64_t max = 0;
    int64_t maxExceptCenter = 0;
    uint64_t totalIterations = 0;

    int autoIterationCount(int currentCount) const;

    void record(int64_t iterations, int x,
        std::optional<int> y = std::nullopt);
    void merge(const RenderIterationStats &other);
};

typedef std::optional<std::reference_wrapper<RenderIterationStats>>
    OptionalIterationStats;