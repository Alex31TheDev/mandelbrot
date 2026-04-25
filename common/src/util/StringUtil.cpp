#include "StringUtil.h"

#include <cctype>
#include <string>
#include <string_view>

namespace StringUtil {
    bool startsWith(std::string_view value, std::string_view prefix) {
        return value.size() >= prefix.size() &&
            value.compare(0, prefix.size(), prefix) == 0;
    }

    bool endsWith(std::string_view value, std::string_view suffix) {
        return value.size() >= suffix.size() &&
            value.substr(value.size() - suffix.size()) == suffix;
    }

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

    std::string_view trimWhitespace(std::string_view value) {
        size_t begin = 0;
        while (begin < value.size() &&
            (value[begin] == ' ' || value[begin] == '\t' ||
                value[begin] == '\n' || value[begin] == '\r' ||
                value[begin] == '\f' || value[begin] == '\v')) {
            ++begin;
        }

        size_t end = value.size();
        while (end > begin &&
            (value[end - 1] == ' ' || value[end - 1] == '\t' ||
                value[end - 1] == '\n' || value[end - 1] == '\r' ||
                value[end - 1] == '\f' || value[end - 1] == '\v')) {
            --end;
        }

        return value.substr(begin, end - begin);
    }

    std::string appendSuffix(std::string_view value,
        std::string_view suffix) {
        if (suffix.empty()) return std::string(value);
        if (endsWith(value, suffix)) {
            return std::string(value);
        }

        std::string out;
        out.reserve(value.size() + suffix.size());
        out.append(value);
        out.append(suffix);
        return out;
    }

    std::string stripSuffix(std::string_view value,
        std::string_view suffix) {
        if (suffix.empty()) return std::string(value);
        if (endsWith(value, suffix)) {
            return std::string(value.substr(0, value.size() - suffix.size()));
        }

        return std::string(value);
    }
}