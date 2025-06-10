#include "ArgsParser.h"

#include <cstdio>
#include <cstring>

#include "Usage.h"
#include "ColorMethods.h"
#include "../util/ParserUtil.h"
using namespace ColorMethods;
using namespace ParserUtil;

#include "../scalar/ScalarTypes.h"

#include "../render/RenderGlobals.h"
#include "../scalar/ScalarGlobals.h"
#include "../vector/VectorGlobals.h"
#include "../mpfr/MpfrGlobals.h"
using namespace RenderGlobals;
using namespace ScalarGlobals;

static const char flagHelp[] = "flag must be \"true\" or \"false\"";

namespace ArgsParser {
    bool checkHelp(int argc, char **argv) {
        if (argsCount(argc) < 1) {
            printUsage(argv[0]);
            return true;
        } else if (argsCount(argc) != 1) {
            return false;
        }

        for (const char *opt : helpOptions) {
            if (strcmp(argv[1], opt) == 0) {
                printUsage(argv[0]);
                return true;
            }
        }

        return false;
    }

    bool parse(int argc, char **argv) {
        if (argsCount(argc) < MIN_ARGS ||
            argsCount(argc) > MAX_ARGS) {
            printUsage(argv[0], true);
            return false;
        }

        int img_w = PARSE_INT32(argv[1]);
        int img_h = PARSE_INT32(argv[2]);

        if (setImageGlobals(img_w, img_h)) {
            initImageValues();
        } else {
            fprintf(stderr, "Invalid args.\nWidth and height must be > 0.\n");
            return false;
        }

        point_r = PARSE_F(argv[3]);
        point_i = PARSE_F(argv[4]);

        float zoomScale = PARSE_H(argv[5]);
        int iterCount = 0;

        if (argc > 6 && strcmp(argv[6], "auto") != 0) {
            iterCount = PARSE_INT32(argv[6]);
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
            int method = parseColorMethod(argv[8]);
            if (method == -1) return false;

            colorMethod = method;
        } else {
            colorMethod = DEFAULT_COLOR_METHOD.id;
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

        seed_r = argc > 11 ? PARSE_F(argv[11]) : DEFAULT_SEED_R;
        seed_i = argc > 12 ? PARSE_F(argv[12]) : DEFAULT_SEED_I;

        scalar_full_t pw = argc > 13 ? PARSE_F(argv[13]) : DEFAULT_FRACTAL_EXP;

        if (!setFractalExponent(pw)) {
            fprintf(stderr, "Invalid args.\nFractal exponent must be > 1.\n");
            return false;
        }

        switch (colorMethod) {
            case 0:
            case 1:
            {
                float R = argc > 14 ? PARSE_H(argv[14]) : DEFAULT_FREQ_R;
                float G = argc > 15 ? PARSE_H(argv[15]) : DEFAULT_FREQ_G;
                float B = argc > 16 ? PARSE_H(argv[16]) : DEFAULT_FREQ_B;
                float mult = argc > 17 ? PARSE_H(argv[17]) : DEFAULT_FREQ_MULT;

                if (!setColorGlobals(R, G, B, mult)) {
                    fprintf(stderr, "Invalid args.\nFrequency multiplier must be non-zero.\n");
                    return false;
                }
            }
            break;

            case 2:
            {
                float real = argc > 14 ? PARSE_H(argv[14]) : DEFAULT_LIGHT_R;
                float imag = argc > 15 ? PARSE_H(argv[15]) : DEFAULT_LIGHT_I;

                if (!setLightGlobals(real, imag)) {
                    fprintf(stderr, "Invalid args.\nLight vector must be non-zero.\n");
                    return false;
                }
            }
            break;
        }

        VectorGlobals::initVectors();
        MpfrGlobals::initMpfrValues(argv[3], argv[4]);

        return true;
    }
}