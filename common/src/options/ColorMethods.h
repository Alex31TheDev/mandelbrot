#pragma once

#include <string_view>

#include "util/RangeUtil.h"

enum class ColorMethod : int {
    iterations = 0,
    smooth_iterations = 1,
    palette = 2,
    light = 3
};

namespace ColorMethods {
    struct ColorMethodOption {
        const char *name;
        int id;
    };

    DECLARE_RANGE_ARRAY(ColorMethodOption, colorMethods);
    const ColorMethodOption DEFAULT_COLOR_METHOD = colorMethods[1];

    int parseColorMethod(std::string_view str);
}
