#pragma once

#include <vector>
#include <string>

namespace ParserUtil {
    bool parseBool(const char *str, bool &ok);
    std::vector<std::string> parseCommandLine(const std::string &cmd);
}
