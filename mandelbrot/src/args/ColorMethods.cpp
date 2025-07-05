#include "ColorMethods.h"

#include <cstdio>
#include <cstring>

#include <array>
#include <algorithm>
#include <sstream>

namespace ColorMethods {
    DEFINE_RANGE_ARRAY(ColorMethod, colorMethods,
        { "iterations", 0 },
        { "smooth_iterations", 1 },
        { "light", 2 }
    );

    int parseColorMethod(const char *str) {
        colorMethodsRange range{};

        const auto it = std::ranges::find_if(range,
            [str](const ColorMethod &m) { return strcmp(str, m.name) == 0; });
        if (it != range.end()) return it->id;

        std::ostringstream methods;
        bool first = true;

        for (const ColorMethod &m : range) {
            methods << (first ? "" : ", ") << m.name;
            first = false;
        }

        fprintf(stderr, "Invalid color method '%s'. Valid options:%s\n",
            str, methods.str().c_str());
        return -1;
    }
}