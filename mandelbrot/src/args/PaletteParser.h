#pragma once

#include <string>
#include <vector>

#include "ArgsUsage.h"

#include "../scalar/ScalarTypes.h"
#include "../scalar/ScalarColors.h"

namespace PaletteParser {
    constexpr char commentToken = '#';

    struct PaletteConfig {
        scalar_half_t totalLength = SC_SYM_H(10.0);
        scalar_half_t offset = ZERO_H;
        std::vector<ScalarPaletteColor> entries;
    };

    bool parse(
        const std::vector<std::string> &args,
        PaletteConfig &out,
        std::string &err
    );
}
