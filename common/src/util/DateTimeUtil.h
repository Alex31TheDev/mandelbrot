#pragma once

#include <time.h>
#include <string>

namespace DateTimeUtil {
    bool tryFormatLocalTime(time_t time, const std::string &format,
        std::string &out);

    std::string formatCurrentLocalTime(const std::string &format);
}
