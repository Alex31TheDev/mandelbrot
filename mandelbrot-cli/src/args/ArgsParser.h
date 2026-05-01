#pragma once

#include <string>

#include "BackendAPI.h"

namespace ArgsParser {
    float parseColorValue(const std::string &str,
        float defaultValue);

    bool checkHelp(int argc, char **argv);
    Backend::Status parse(Backend::Session &session, int argc, char **argv);
}
