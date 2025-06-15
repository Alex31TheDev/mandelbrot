#include "TimeUtil.h"

#include <cstdint>
#include <cmath>

#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>

static const char *units[] = { "ms", "s", "min", "h", "d" };
constexpr int unitCount = sizeof(units) / sizeof(units[0]);

static const double factors[] = { 1000.0, 60.0, 60.0, 24.0 };
constexpr int maxSteps = sizeof(factors) / sizeof(factors[0]);

namespace TimeUtil {
    std::string formatTime(int64_t millis) {
        std::ostringstream oss;
        oss << std::fixed;

        if (millis <= 0) {
            oss << millis << " " << units[0];
            return oss.str();
        }

        int unitIdx = 0;
        double count = static_cast<double>(millis);

        while (unitIdx < maxSteps && count >= factors[unitIdx]) {
            count /= factors[unitIdx];
            unitIdx++;
        }

        unitIdx = std::min(unitIdx, unitCount - 1);

        if (count == std::floor(count)) {
            oss << std::setprecision(0);
        } else {
            oss << std::setprecision(1);
        }

        oss << count << " " << units[unitIdx];
        return oss.str();
    }
}