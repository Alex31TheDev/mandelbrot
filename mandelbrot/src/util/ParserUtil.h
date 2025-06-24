#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <tuple>
#include <type_traits>

class ArgsVec {
public:
    int argc;
    char **argv;

    static ArgsVec fromParsed(char *progName,
        std::vector<std::string> &&parsedArgs);

    ~ArgsVec();

    ArgsVec(const ArgsVec &) = delete;
    ArgsVec &operator=(const ArgsVec &) = delete;

    ArgsVec(ArgsVec &&) = default;
    ArgsVec &operator=(ArgsVec &&) = default;

private:
    ArgsVec(int count);
};

namespace ParserUtil {
    bool insensitiveCompare(std::string_view str, std::string_view target);
    bool parseBool(std::string_view input, bool *ok = nullptr);

    template<typename T, int base = 10> requires std::is_arithmetic_v<T>
    T parseNumber(const std::string &input, bool *ok,
        const T defaultValue = T());
    template<typename T> requires std::is_arithmetic_v<T>
    T parseNumber(const std::string &input, const T defaultValue = T()) {
        return parseNumber<T, 10>(input, nullptr, defaultValue);
    }

    template<typename T, int base = 10> requires std::is_arithmetic_v<T>
    T parseNumber(int argc, char *argv[], int index, bool *ok,
        const T defaultValue = T());
    template<typename T> requires std::is_arithmetic_v<T>
    T parseNumber(int argc, char *argv[], int index,
        const T defaultValue = T()) {
        return parseNumber<T, 10>(argc, argv, index, nullptr, defaultValue);
    }

    std::tuple<float, float, float>
        parseHexColor(std::string_view str, bool *ok = nullptr);

    std::vector<std::string> parseCommandLine(const std::string &cmd);
}

#include "ParserUtil_impl.h"