#include "FractalTypes.h"

#include <string_view>
#include <algorithm>

#include "util/RangeUtil.h"

namespace FractalTypes {
    DEFINE_RANGE_ARRAY(FractalType, fractalTypes,
        { "mandelbrot", 0 },
        { "perpendicular", 1 },
        { "burningship", 2 }
    );

    int parseFractalType(std::string_view str) {
        fractalTypesRange range{};
        const auto it = std::ranges::find_if(range,
            [str](const FractalType &type) { return str == type.name; });

        return (it != range.end()) ? it->id : -1;
    }
}