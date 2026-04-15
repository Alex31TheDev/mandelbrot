#include "StringUtil.h"

#include <cctype>

namespace StringUtil {
    std::string toLower(std::string_view str) {
        std::string out(str.size(), '\0');

        for (size_t i = 0; i < str.size(); ++i) {
            out[i] = static_cast<char>(
                std::tolower(static_cast<unsigned char>(str[i])));
        }

        return out;
    }

    std::string toUpper(std::string_view str) {
        std::string out(str.size(), '\0');

        for (size_t i = 0; i < str.size(); ++i) {
            out[i] = static_cast<char>(
                std::toupper(static_cast<unsigned char>(str[i])));
        }

        return out;
    }
}