#include "ArgsParser.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "Usage.h"
#include "ParserUtil.h"
#include "ColorMethods.h"
using namespace ParserUtil;

#include "../scalar/ScalarGlobals.h"
using namespace ScalarGlobals;

#ifdef __AVX2__
#include "../vector/VectorGlobals.h"
using namespace VectorGlobals;
#endif

static const char flagHelp[] = "flag must be \"true\" or \"false\"";

static void setArgs_vec() {
#ifdef __AVX2__
    initVectors();
#endif
}

namespace ArgsParser {
    bool parse(int argc, char **argv) {
        int argsCount = argc - 1;

        if (argsCount < MIN_ARGS || argsCount > MAX_ARGS) {
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
        int iterCount = 0;

        if (argc > 6 && strcmp(argv[6], "auto") != 0) {
            iterCount = strtol(argv[6], nullptr, 10);
        }

        if (!setZoomGlobals(iterCount, zoomScale)) {
            fprintf(stderr, "Invalid args.\nScale must be > -3.25.\n");
            return false;
        }

        if (argc > 7) {
            bool ok;
            useThreads = parseBool(argv[7], ok);

            if (!ok) {
                fprintf(stderr, "Invalid args.\nThreads %s.\n", flagHelp);
                return false;
            }
        }

        if (argc > 8) {
            int method = ColorMethods::parseColorMethod(argv[8]);
            if (method == -1) return false;
            colorMethod = method;
        } else {
            colorMethod = ColorMethods::colorMethods[0].id;
        }

        if (argc > 9) {
            bool ok;
            isJuliaSet = parseBool(argv[9], ok);

            if (!ok) {
                fprintf(stderr, "Invalid args.\nJulia set %s.\n", flagHelp);
                return false;
            }
        }

        if (argc > 10) {
            bool ok;
            isInverse = parseBool(argv[10], ok);

            if (!ok) {
                fprintf(stderr, "Invalid args.\nInverse %s.\n", flagHelp);
                return false;
            }
        }

        seed_r = argc > 11 ? strtod(argv[11], nullptr) : DEFAULT_SEED_R;
        seed_i = argc > 12 ? strtod(argv[12], nullptr) : DEFAULT_SEED_I;

        switch (colorMethod) {
            case 0:
            {
                float R = argc > 13 ? strtof(argv[13], nullptr) : DEFAULT_FREQ_R;
                float G = argc > 14 ? strtof(argv[14], nullptr) : DEFAULT_FREQ_G;
                float B = argc > 15 ? strtof(argv[15], nullptr) : DEFAULT_FREQ_B;
                float mult = argc > 16 ? strtof(argv[16], nullptr) : DEFAULT_FREQ_MULT;

                if (!setColorGlobals(R, G, B, mult)) {
                    fprintf(stderr, "Invalid args.\nFrequency multiplier must be non-zero.\n");
                    return false;
                }
            }
            break;

            case 1:
            {
                float real = argc > 13 ? strtof(argv[13], nullptr) : DEFAULT_LIGHT_R;
                float imag = argc > 14 ? strtof(argv[14], nullptr) : DEFAULT_LIGHT_I;

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