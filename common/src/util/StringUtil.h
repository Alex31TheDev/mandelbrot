#pragma once

#include <string>
#include <string_view>

namespace StringUtil {
    std::string toLower(std::string_view value);
    std::string toUpper(std::string_view value);
}
