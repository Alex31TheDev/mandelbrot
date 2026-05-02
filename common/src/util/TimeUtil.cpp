#include "TimeUtil.h"

#include <chrono>

using namespace std::chrono;

static bool safeLocaltime(time_t time, tm &out) {
#if defined(_MSC_VER)
    return localtime_s(&out, &time) == 0;
#else
    return localtime_r(&time, &out) != nullptr;
#endif
}

namespace TimeUtil {
    bool tryFormatLocalTime(
        time_t time, const std::string &format,
        std::string &out
    ) {
        tm localTime;
        if (!safeLocaltime(time, localTime)) return false;

        char buf[64] = { '\0' };
        const size_t written = strftime(buf, sizeof(buf),
            format.c_str(), &localTime);

        if (written == 0) {
            out.clear();
            return false;
        }

        out.assign(buf, written);
        return true;
    }

    std::string formatCurrentTime(const std::string &format) {
        const time_t now = system_clock::to_time_t(system_clock::now());

        std::string formatted;
        if (!tryFormatLocalTime(now, format, formatted)) return "";

        return formatted;
    }
}