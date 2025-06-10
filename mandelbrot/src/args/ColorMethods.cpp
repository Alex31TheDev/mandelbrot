#include "ColorMethods.h"

#include <cstdio>
#include <cstring>

namespace ColorMethods {
    const ColorMethod colorMethods[] = {
        { "iterations", 0 },
        { "smooth_iterations", 1 },
        { "light", 2 },
    };

    const ColorMethod DEFAULT_COLOR_METHOD = colorMethods[1];

    int parseColorMethod(const char *str) {
        for (const ColorMethod &method : colorMethods) {
            if (strcmp(str, method.name) == 0) return method.id;
        }

        fprintf(stderr, "Invalid colorMethod '%s'. Valid options: ", str);

        bool first = true;
        for (const ColorMethod &method : colorMethods) {
            fprintf(stderr, "%s%s", first ? "" : ", ", method.name);
            first = false;
        }

        fprintf(stderr, "\n");
        return -1;
    }
}