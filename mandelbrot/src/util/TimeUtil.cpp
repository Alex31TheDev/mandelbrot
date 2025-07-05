#include "TimeUtil.h"

#include <cstdint>
#include <sstream>
#include <string>

static const char *units[] = { "ms", "s", "min", "h", "d" };
constexpr int unitCount = sizeof(units) / sizeof(units[0]);

static const int64_t factors[] = { 1000i64, 60i64, 60i64, 24i64 };
constexpr int maxSteps = sizeof(factors) / sizeof(factors[0]);

namespace TimeUtil {
    std::string formatTime(int64_t millis) {
        std::ostringstream oss;

        if (millis <= 0) {
            oss << millis << " " << units[0];
            return oss.str();
        }

        int64_t remainder = 0;
        int unitIdx = 0;

        while (unitIdx < maxSteps && millis >= factors[unitIdx]) {
            remainder = millis % factors[unitIdx];
            millis /= factors[unitIdx];
            unitIdx++;
        }

        unitIdx = unitIdx < unitCount ? unitIdx : unitCount - 1;

        oss << millis;
        if (remainder != 0 && unitIdx > 0) {
            const int64_t decimal = (remainder * 10) / factors[unitIdx - 1];
            if (decimal != 0) oss << "." << decimal;
        }

        oss << " " << units[unitIdx];
        return oss.str();
    }
}