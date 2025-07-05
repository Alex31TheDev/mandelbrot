#include "ArgsParser.h"

#include <cstdio>
#include <cstring>

#include <tuple>
#include <algorithm>

#include "Usage.h"
#include "ColorMethods.h"
using namespace ColorMethods;

#include "../scalar/ScalarTypes.h"

#include "../render/RenderGlobals.h"
#include "../scalar/ScalarGlobals.h"
#include "../vector/VectorGlobals.h"
#include "../mpfr/MPFRGlobals.h"
using namespace RenderGlobals;
using namespace ScalarGlobals;

#include "../util/ParserUtil.h"
using namespace ParserUtil;

#define PARSE_NUM(idx, default) \
    parseNumber(argc, argv, idx, DEFAULT_ ## default)

static const char *flagHelp = "flag must be \"true\" or \"false\"";

namespace ArgsParser {
    bool checkHelp(int argc, char **argv) {
        if (!argv) return false;
        const char *progPath = argv[0];

        if (argsCount(argc) < 1) {
            printUsage(progPath);
            return true;
        } else if (argsCount(argc) != 1) {
            return false;
        }

        const char *helpArg = argv[1];
        const bool isHelp = std::ranges::any_of(helpOptionsRange{},
            [helpArg](const char *opt) { return strcmp(helpArg, opt) == 0; });

        if (isHelp) printUsage();
        return isHelp;
    }

    bool parse(int argc, char **argv) {
        if (!argv) return false;

        if (argsCount(argc) < MIN_ARGS ||
            argsCount(argc) > MAX_ARGS) {
            printUsage(argv[0], true);
            return false;
        }

        const int img_w = PARSE_INT32(argv[1]);
        const int img_h = PARSE_INT32(argv[2]);

        if (setImageGlobals(img_w, img_h)) {
            initImageValues();
        } else {
            fprintf(stderr, "Invalid args.\nWidth and height must be > 0.\n");
            return false;
        }

        const scalar_full_t pr = PARSE_F(argv[3]);
        const scalar_full_t pi = PARSE_F(argv[4]);

        const float zoomScale = PARSE_H(argv[5]);
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
            useThreads = parseBool(argv[7], &ok);

            if (!ok) {
                fprintf(stderr, "Invalid args.\nThreads %s.\n", flagHelp);
                return false;
            }
        }

        if (argc > 8) {
            const int method = parseColorMethod(argv[8]);
            if (method == -1) return false;

            colorMethod = method;
        } else {
            colorMethod = DEFAULT_COLOR_METHOD.id;
        }

        bool julia = false, inverse = false;

        if (argc > 9) {
            bool ok;
            julia = parseBool(argv[9], &ok);

            if (!ok) {
                fprintf(stderr, "Invalid args.\nJulia set %s.\n", flagHelp);
                return false;
            }
        }

        if (argc > 10) {
            bool ok;
            inverse = parseBool(argv[10], &ok);

            if (!ok) {
                fprintf(stderr, "Invalid args.\nInverse %s.\n", flagHelp);
                return false;
            }
        }

        setFractalType(julia, inverse);

        const scalar_full_t sr = PARSE_NUM(11, SEED_R);
        const scalar_full_t si = PARSE_NUM(12, SEED_I);

        setZoomPoints(pr, pi, sr, si);

        const scalar_full_t pw = PARSE_NUM(13, FRACTAL_EXP);

        if (!setFractalExponent(pw)) {
            fprintf(stderr, "Invalid args.\nFractal exponent must be > 1.\n");
            return false;
        }

        switch (colorMethod) {
            case 0:
            case 1:
            {
                const scalar_half_t R = PARSE_NUM(14, FREQ_R);
                const scalar_half_t G = PARSE_NUM(15, FREQ_G);
                const scalar_half_t B = PARSE_NUM(16, FREQ_B);
                const scalar_half_t mult = PARSE_NUM(17, FREQ_MULT);

                if (!setColorGlobals(R, G, B, mult)) {
                    fprintf(stderr, "Invalid args.\n"
                        "Frequency multiplier must be non-zero.\n");
                    return false;
                }
            }
            break;

            case 2:
            {
                const scalar_half_t real = PARSE_NUM(14, LIGHT_R);
                const scalar_half_t imag = PARSE_NUM(15, LIGHT_I);

                if (!setLightGlobals(real, imag)) {
                    fprintf(stderr, "Invalid args.\n"
                        "Light vector must be non-zero.\n");
                    return false;
                }
            }
            break;

            default:
                return false;
        }

        VectorGlobals::initVectors();
        MPFRGlobals::initMPFRValues(argv[3], argv[4]);

        return true;
    }
}