#include "FormatUtil.h"

#include <cstdint>
#include <cmath>

#include <algorithm>
#include <string>
#include <string_view>
#include <functional>
#include <iomanip>
#include <iterator>
#include <sstream>

static const char *sizeUnits[] = { "B", "KB", "MB", "GB", "TB" };
static const char *timeUnits[] = { "ms", "s", "min", "h", "d" };
static const int64_t durFactors[] = { 1000i64, 60i64, 60i64, 24i64 };

static int toChannelByte(float value) {
    const float clamped = std::clamp(value, 0.0f, 1.0f);
    return static_cast<int>(clamped * 255.0f + 0.5f);
}

namespace FormatUtil {
    std::string formatBool(bool value) {
        return value ? "true" : "false";
    }

    std::string formatBufferSize(size_t size) {
        std::ostringstream oss;
        oss << std::fixed;

        if (size == 0) {
            oss << "0 " << sizeUnits[0];
            return oss.str();
        }

        int unitIdx = 0;
        double count = static_cast<double>(size);

        while (count >= 1024 && unitIdx < std::size(sizeUnits) - 1) {
            count /= 1024;
            unitIdx++;
        }

        oss << (count == floor(count) ?
            std::setprecision(0) : std::setprecision(1));

        oss << count << " " << sizeUnits[unitIdx];
        return oss.str();
    }

    std::string formatDuration(int64_t millis) {
        std::ostringstream oss;

        if (millis <= 0) {
            oss << millis << " " << timeUnits[0];
            return oss.str();
        }

        int64_t remainder = 0;
        int unitIdx = 0;

        while (unitIdx < std::size(durFactors) &&
            millis >= durFactors[unitIdx]) {
            remainder = millis % durFactors[unitIdx];
            millis /= durFactors[unitIdx];
            unitIdx++;
        }

        unitIdx = unitIdx < std::size(timeUnits) ? unitIdx
            : std::size(timeUnits) - 1;

        oss << millis;
        if (remainder != 0 && unitIdx > 0) {
            const int64_t decimal = (remainder * 10) /
                durFactors[unitIdx - 1];
            if (decimal != 0) oss << "." << decimal;
        }

        oss << " " << timeUnits[unitIdx];
        return oss.str();
    }

    std::string formatHexColor(float r, float g, float b) {
        std::ostringstream oss;

        oss << "#" << std::uppercase << std::hex << std::setfill('0')
            << std::setw(2) << toChannelByte(r)
            << std::setw(2) << toChannelByte(g)
            << std::setw(2) << toChannelByte(b);

        return oss.str();
    }

    std::string formatIndexedName(std::string_view baseName, int index) {
        std::ostringstream oss;
        oss << baseName << ' ' << index;
        return oss.str();
    }

    std::string uniqueIndexedName(
        std::string_view baseName,
        const std::function<bool(std::string_view)> &nameExists,
        int startIndex
    ) {
        if (startIndex < 1) startIndex = 1;

        for (int suffix = startIndex;; suffix++) {
            const std::string candidate = formatIndexedName(baseName, suffix);
            if (!nameExists(candidate)) {
                return candidate;
            }
        }
    }
}