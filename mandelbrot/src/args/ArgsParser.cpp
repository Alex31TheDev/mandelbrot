#include "ArgsParser.h"

#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <cstring>

#include "Usage.h"
#include "ColorMethods.h"

#include "../scalar/ScalarGlobals.h"
using namespace ScalarGlobals;

#ifdef __AVX2__
#include "../vector/VectorGlobals.h"
using namespace VectorGlobals;
#endif

namespace {
    const char flagHelp[] = "flag must be \"true\" or \"false\"";

    bool parseBool(const char *str, bool &ok) {
        if (!str) {
            ok = false;
            return false;
        }

        size_t len = strlen(str);

        if (len != 4 && len != 5) {
            ok = false;
            return false;
        }

        char lower[6] = { 0 };

        for (int i = 0; i < 5 && str[i]; i++) {
            lower[i] = tolower(str[i]);
        }

        if (strcmp(lower, "true") == 0) {
            ok = true;
            return true;
        } else if (strcmp(lower, "false") == 0) {
            ok = true;
            return false;
        }

        ok = false;
        return false;
    }

    void setArgs_vec() {
#ifdef __AVX2__
        initVectors();
#endif
    }
}

namespace ArgsParser {
    bool parse(int argc, char **argv) {
        if (argc < MIN_ARGS || argc > MAX_ARGS) {
            printUsage();
            return false;
        }

        int img_w = strtol(argv[1], nullptr, 10);
        int img_h = strtol(argv[2], nullptr, 10);

        if (!setImageGlobals(img_w, img_h)) {
            fprintf(stderr, "Invalid args.\nWidth and height must be > 0.\n");
            return false;
        }

        point_r = strtod(argv[3], nullptr);
        point_i = strtod(argv[4], nullptr);

        float zoomScale = strtof(argv[5], nullptr);

        if (!setZoomGlobals(zoomScale)) {
            fprintf(stderr, "Invalid args.\nScale must be > 0.\n");
            return false;
        }

        if (argc > 6) {
            int method = ColorMethods::parseColorMethod(argv[6]);
            if (method == -1) return false;
            colorMethod = method;
        } else {
            colorMethod = ColorMethods::colorMethods[0].id;
        }

        if (argc > 7) {
            bool ok;
            isJuliaSet = parseBool(argv[7], ok);

            if (!ok) {
                fprintf(stderr, "Invalid args.\nJulia set %s.\n", flagHelp);
                return false;
            }
        }

        if (argc > 8) {
            bool ok;
            isInverse = parseBool(argv[8], ok);

            if (!ok) {
                fprintf(stderr, "Invalid args.\nInverse %s.\n", flagHelp);
                return false;
            }
        }

        seed_r = argc > 9 ? strtod(argv[9], nullptr) : DEFAULT_SEED_R;
        seed_i = argc > 10 ? strtod(argv[10], nullptr) : DEFAULT_SEED_I;

        switch (colorMethod) {
            case 0:
            {
                float R = argc > 11 ? strtof(argv[11], nullptr) : DEFAULT_FREQ_R;
                float G = argc > 12 ? strtof(argv[12], nullptr) : DEFAULT_FREQ_G;
                float B = argc > 13 ? strtof(argv[13], nullptr) : DEFAULT_FREQ_B;
                float mult = argc > 14 ? strtof(argv[14], nullptr) : DEFAULT_FREQ_MULT;

                if (!setColorGlobals(R, G, B, mult)) {
                    fprintf(stderr, "Invalid args.\nFrequency multiplier must be non-zero.\n");
                    return false;
                }
            }
            break;

            case 1:
            {
                float real = argc > 11 ? strtof(argv[11], nullptr) : DEFAULT_LIGHT_R;
                float imag = argc > 12 ? strtof(argv[12], nullptr) : DEFAULT_LIGHT_I;

                if (!setLightGlobals(real, imag)) {
                    fprintf(stderr, "Invalid args.\nLight vector must be non-zero.\n");
                    return false;
                }
            }
            break;
        }

        setArgs_vec();
        return true;
    }
}