#pragma once

#include <string>

#include "../scalar/ScalarTypes.h"

namespace ArgsParser {
    scalar_half_t parseColorValue(
        const std::string &str,
        scalar_half_t defaultValue
    );

    bool checkHelp(int argc, char **argv);
    bool parse(int argc, char **argv);
}
