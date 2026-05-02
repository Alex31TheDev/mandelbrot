#pragma once

#include <string>

#include "BackendAPI.h"

namespace ArgsParser {
    using namespace Backend;

    float parseColorValue(const std::string &str,
        float defaultValue);

    bool checkHelp(int argc, char **argv);
    Status parse(Session &session, int argc, char **argv);
}
