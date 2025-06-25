#pragma once

#include <string_view>

#include "ScalarTypes.h"

struct ScalarColor {
    scalar_half_t R, G, B;

    static ScalarColor fromString(std::string_view hex);
};

struct ScalarPaletteColor : public ScalarColor {
    scalar_half_t length;

    static ScalarColor fromHex(std::string_view) = delete;

    static ScalarPaletteColor fromString
    (std::string_view hex, std::string_view lengthStr);
};
