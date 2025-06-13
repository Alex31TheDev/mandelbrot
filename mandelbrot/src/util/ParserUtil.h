#pragma once

#include <vector>
#include <string>

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
    bool parseBool(const char *str, bool &ok);
    std::vector<std::string> parseCommandLine(const std::string &cmd);
}
