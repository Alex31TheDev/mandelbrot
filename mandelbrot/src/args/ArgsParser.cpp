#include "ArgsParser.h"

#include <cstdlib>
#include <cstdio>

#include "Usage.h"
#include "ColorMethods.h"

#include "../scalar/ScalarGlobals.h"
using namespace ScalarGlobals;

#ifdef __AVX2__
#include "../vector/VectorGlobals.h"
using namespace VectorGlobals;
#endif

namespace {
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

        switch (colorMethod) {
            case 0:
            {
                float R = argc > 7 ? strtof(argv[7], nullptr) : DEFAULT_FREQ_R;
                float G = argc > 8 ? strtof(argv[8], nullptr) : DEFAULT_FREQ_G;
                float B = argc > 9 ? strtof(argv[9], nullptr) : DEFAULT_FREQ_B;
                float mult = argc > 10 ? strtof(argv[9], nullptr) : DEFAULT_FREQ_MULT;

                if (!setColorGlobals(R, G, B, mult)) {
                    fprintf(stderr, "Invalid args.\nFrequency multiplier must be non-zero.\n");
                    return false;
                }
            }
            break;

            case 1:
            {
                float real = argc > 7 ? strtof(argv[7], nullptr) : DEFAULT_LIGHT_R;
                float imag = argc > 8 ? strtof(argv[8], nullptr) : DEFAULT_LIGHT_I;

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