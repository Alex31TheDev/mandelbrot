#include "RenderIterationStats.h"

#include <algorithm>

#include "RenderGlobals.h"
using namespace RenderGlobals;

int RenderIterationStats::autoIterationCount(int currentCount) const {
    if (!valid) return currentCount;

    int64_t nextCount = currentCount;
    if (nextCount < max) nextCount = max;

    if (nextCount < min + min + 2000) {
        nextCount = min + min + 3000;
    }

    if (maxExceptCenter < nextCount / 3 &&
        maxExceptCenter * 3 > 1000) {
        nextCount = maxExceptCenter * 3;
    }

    return static_cast<int>(nextCount);
}

void RenderIterationStats::record(
    int64_t iterations, int x,
    std::optional<int> y
) {
    if (!valid) {
        valid = true;
        min = iterations;
        max = iterations;
    } else {
        min = std::min(min, iterations);
        max = std::max(max, iterations);
    }

    totalIterations += static_cast<uint64_t>(iterations) + 1;
    if (!y) return;

    const int centerXOuter = width / 2 - 2,
        centerYOuter = height / 2 - 2;

    if ((x < centerXOuter || x > width - centerXOuter) &&
        (*y < centerYOuter || *y > 3 * centerYOuter)) {
        maxExceptCenter = std::max(maxExceptCenter, iterations);
    }
}

void RenderIterationStats::merge(const RenderIterationStats &other) {
    totalIterations += other.totalIterations;

    if (!other.valid) return;
    if (!valid) {
        const uint64_t total = totalIterations;
        *this = other;
        totalIterations = total;
        return;
    }

    min = std::min(min, other.min);
    max = std::max(max, other.max);
    maxExceptCenter = std::max(maxExceptCenter, other.maxExceptCenter);
}