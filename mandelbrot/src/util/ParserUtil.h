#pragma once

#include <string>
#include <string_view>
#include <vector>

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
    bool parseBool(std::string_view input, bool *ok = nullptr);

    template<typename T>
    T parseNumber(const std::string &input, bool *ok = nullptr,
        const T defaultValue = T());
    template<typename T>
    T parseNumber(const std::string &input,
        const T defaultValue = T()) {
        return parseNumber<T>(input, nullptr, defaultValue);
    }

    template<typename T>
    T parseNumber(int argc, char *argv[], int index, bool *ok = nullptr,
        const T defaultValue = T());
    template<typename T>
    T parseNumber(int argc, char *argv[], int index,
        const T defaultValue = T()) {
        return parseNumber<T>(argc, argv, index, nullptr, defaultValue);
    }

    std::vector<std::string> parseCommandLine(const std::string &cmd);
}

#include "ParserUtil_impl.h"