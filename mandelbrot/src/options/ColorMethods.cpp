#include "ColorMethods.h"

#include <algorithm>

namespace ColorMethods {
    DEFINE_RANGE_ARRAY(ColorMethod, colorMethods,
        { "iterations", 0 },
        { "smooth_iterations", 1 },
        { "light", 2 }
    );

    int parseColorMethod(std::string_view str) {
        colorMethodsRange range{};
        const auto it = std::ranges::find_if(range,
            [str](const ColorMethod &method) { return str == method.name; });

        return (it != range.end()) ? it->id : -1;
    }
}
