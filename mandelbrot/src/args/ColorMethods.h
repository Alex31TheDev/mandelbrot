#pragma once

namespace ColorMethods {
    struct ColorMethod {
        const char *name;
        int id;
    };

    extern const ColorMethod colorMethods[];
    int parseColorMethod(const char *str);
}
