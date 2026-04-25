#pragma once

#include <string>
#include <string_view>

namespace StringUtil {
    bool startsWith(std::string_view value, std::string_view prefix);
    bool endsWith(std::string_view value, std::string_view suffix);

    std::string toLower(std::string_view value);
    std::string toUpper(std::string_view value);

    std::string_view trimWhitespace(std::string_view value);

    std::string appendSuffix(std::string_view value,
        std::string_view suffix);
    std::string stripSuffix(std::string_view value,
        std::string_view suffix);
}
