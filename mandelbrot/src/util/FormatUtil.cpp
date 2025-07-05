#include "FormatUtil.h"

#include <cstdint>
#include <cmath>

#include <sstream>
#include <string>
#include <iomanip>

static const char *sizeUnits[] = { "B", "KB", "MB", "GB", "TB" };
constexpr int sizeUnitCount = sizeof(sizeUnits) / sizeof(sizeUnits[0]);

static const char *timeUnits[] = { "ms", "s", "min", "h", "d" };
constexpr int timeUnitCount = sizeof(timeUnits) / sizeof(timeUnits[0]);

static const int64_t durFactors[] = { 1000i64, 60i64, 60i64, 24i64 };
constexpr int durMaxSteps = sizeof(durFactors) / sizeof(durFactors[0]);

namespace FormatUtil {
    std::string formatBufferSize(size_t size) {
        std::ostringstream oss;
        oss << std::fixed;

        if (size == 0) {
            oss << "0 " << sizeUnits[0];
            return oss.str();
        }

        int unitIdx = 0;
        double count = static_cast<double>(size);

        while (count >= 1024 && unitIdx < sizeUnitCount - 1) {
            count /= 1024;
            unitIdx++;
        }

        oss << (count == floor(count) ?
            std::setprecision(0) : std::setprecision(1));

        oss << count << " " << sizeUnits[unitIdx];
        return oss.str();
    }

    std::string formatDuration(int64_t millis, int decimals) {
        std::ostringstream oss;

        if (millis <= 0) {
            oss << millis << " " << timeUnits[0];
            return oss.str();
        }

        int64_t remainder = 0;
        int unitIdx = 0;

        while (unitIdx < durMaxSteps &&
            millis >= durFactors[unitIdx]) {
            remainder = millis % durFactors[unitIdx];
            millis /= durFactors[unitIdx];
            unitIdx++;
        }

        unitIdx = unitIdx < timeUnitCount ? unitIdx : timeUnitCount - 1;

        oss << millis;
        if (remainder != 0 && unitIdx > 0) {
            const int64_t decimal = (remainder * 10) /
                durFactors[unitIdx - 1];
            if (decimal != 0) oss << "." << decimal;
        }

        oss << " " << timeUnits[unitIdx];
        return oss.str();
    }
}