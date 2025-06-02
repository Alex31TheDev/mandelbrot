#include "ColorMethods.h"

#include <cstdio>
#include <cstring>

namespace ColorMethods {
    const ColorMethod colorMethods[] = {
        { "iterations", 0 },
        { "smooth_iterations", 1 },
        { "light", 2 },
        { nullptr, -1 }
    };

    int parseColorMethod(const char *str) {
        for (const ColorMethod *method = colorMethods; method->name != nullptr; method++) {
            if (strcmp(str, method->name) == 0) return method->id;
        }

        fprintf(stderr, "Invalid colorMethod '%s'. Valid options: ", str);

        for (const ColorMethod *method = colorMethods; method->name != nullptr; method++) {
            bool hasNext = (method + 1)->name != nullptr;
            fprintf(stderr, "%s%s", method->name, hasNext ? ", " : "\n");
        }

        return -1;
    }
}