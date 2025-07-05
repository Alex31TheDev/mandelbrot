#pragma once

#include "../util/RangeUtil.h"

namespace ColorMethods {
    struct ColorMethod {
        const char *name;
        int id;
    };

    DECLARE_RANGE_ARRAY(ColorMethod, colorMethods);
    const ColorMethod DEFAULT_COLOR_METHOD = colorMethods[1];

    int parseColorMethod(const char *str);
}
