#pragma once

namespace ColorMethods {
    struct ColorMethod {
        const char *name;
        int id;
    };

    extern const ColorMethod colorMethods[];
    extern const ColorMethod DEFAULT_COLOR_METHOD;

    int parseColorMethod(const char *str);
}
