#include "BufferUtil.h"

#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>

static const char *units[] = { "B", "KB", "MB", "GB", "TB" };
constexpr int unitCount = sizeof(units) / sizeof(units[0]);

namespace BufferUtil {
    std::string formatSize(size_t size) {
        std::ostringstream oss;
        oss << std::fixed;

        if (size == 0) {
            oss << "0 " << units[0];
            return oss.str();
        }

        int unitIdx = 0;
        double count = static_cast<double>(size);

        while (count >= 1024 && unitIdx < unitCount - 1) {
            count /= 1024;
            unitIdx++;
        }

        if (count == floor(count)) {
            oss << std::setprecision(0);
        } else {
            oss << std::setprecision(1);
        }

        oss << count << " " << units[unitIdx];
        return oss.str();
    }
}