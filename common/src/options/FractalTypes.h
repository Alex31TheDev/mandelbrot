#pragma once

#include <string_view>

#include "util/RangeUtil.h"

namespace FractalTypes {
    struct FractalType {
        const char *name;
        int id;
    };

    DECLARE_RANGE_ARRAY(FractalType, fractalTypes);
    const FractalType DEFAULT_FRACTAL_TYPE = fractalTypes[0];

    int parseFractalType(std::string_view str);
}
