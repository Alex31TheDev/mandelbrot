#pragma once

#include <string>
#include <string_view>

#include "IncludeWin32.h"

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

    std::wstring utf8ToWide(std::string_view value);

#ifdef _WIN32
    void copyToClipboard(HWND hwnd, const std::wstring &text);
    void copyToClipboard(HWND hwnd, std::string_view text);
#endif
}
