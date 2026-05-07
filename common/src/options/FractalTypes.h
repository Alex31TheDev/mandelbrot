#pragma once

#include <string_view>

#include "util/RangeUtil.h"

enum class FractalType : int {
    mandelbrot = 0,
    perpendicular = 1,
    burningship = 2
};

namespace FractalTypes {
    struct FractalTypeOption {
        const char *name;
        int id;
    };

    DECLARE_RANGE_ARRAY(FractalTypeOption, fractalTypes);
    const FractalTypeOption DEFAULT_FRACTAL_TYPE = fractalTypes[0];

    int parseFractalType(std::string_view str);
}
