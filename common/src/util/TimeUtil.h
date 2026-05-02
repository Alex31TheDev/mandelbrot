#pragma once

#include <time.h>
#include <string>

namespace TimeUtil {
    bool tryFormatLocalTime(time_t time, const std::string &format,
        std::string &out);

    std::string formatCurrentTime(const std::string &format);
}
