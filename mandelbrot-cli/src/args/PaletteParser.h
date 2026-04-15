#pragma once

#include <string>
#include <vector>

#include "BackendApi.h"
#include "ArgsUsage.h"

namespace PaletteParser {
    constexpr char commentToken = '#';

    bool parse(
        const std::vector<std::string> &args,
        Backend::PaletteConfig &out,
        std::string &err
    );
}
