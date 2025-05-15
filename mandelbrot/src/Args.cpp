#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "Args.h"

#include "GlobalsScalar.h"
#ifdef __AVX2__
#include "GlobalsVector.h"
#endif

const char usage[] =
"Usage: ./mandelbrot width height point_r point_i scale "
"[colorMethod] [freq_r/light_r] [freq_g/light_i] [freq_b] [freqMult]";

struct ColorMethod {
    const char *name;
    int id;
};

const ColorMethod colorMethods[] = {
   { "smooth_iterations", 0 },
   { "light", 1 },
   { NULL, -1 }
};

int parseColorMethod(const char *str) {
    for (const ColorMethod *method = colorMethods; method->name != NULL; method++) {
        if (strcmp(str, method->name) == 0) return method->id;
    }

    fprintf(stderr, "Invalid colorMethod '%s'. Valid options: ", str);

    for (const ColorMethod *method = colorMethods; method->name != NULL; method++) {
        bool hasNext = (method + 1)->name != NULL;
        fprintf(stderr, "%s%s", method->name, hasNext ? ", " : "\n");
    }

    return -1;
}

void setArgs_vec() {
#ifdef __AVX2__
    initVectors();
#endif
}

bool parseArgs(int argc, char **argv) {
    if (argc < 6 || argc > 11) {
        fprintf(stderr, "Invalid args.\n%s\n", usage);
        return false;
    }

    int img_w = strtol(argv[1], NULL, 10);
    int img_h = strtol(argv[2], NULL, 10);

    if (!setImageGlobals(img_w, img_h)) {
        fprintf(stderr, "Invalid args.\nWidth and height must be > 0.\n");
        return false;
    }

    point_r = strtod(argv[3], NULL);
    point_i = strtod(argv[4], NULL);

    double zoomScale = strtod(argv[5], NULL);

    if (!setScaleGlobals(zoomScale)) {
        fprintf(stderr, "Invalid args.\nScale must be > 0.\n");
        return false;
    }

    if (argc > 6) {
        int method = parseColorMethod(argv[6]);
        if (method == -1) return false;
        colorMethod = method;
    } else {
        colorMethod = colorMethods[0].id;
    }

    switch (colorMethod) {
        case 0:
        {
            float R = argc > 7 ? strtof(argv[7], NULL) : DEFAULT_FREQ_R;
            float G = argc > 8 ? strtof(argv[8], NULL) : DEFAULT_FREQ_G;
            float B = argc > 9 ? strtof(argv[9], NULL) : DEFAULT_FREQ_B;
            float mult = argc > 10 ? strtof(argv[9], NULL) : DEFAULT_FREQ_MULT;

            if (!setColorGlobals(R, G, B, mult)) {
                fprintf(stderr, "Invalid args.\nFrequency multiplier must be non-zero.\n");
                return false;
            }
        }
        break;

        case 1:
        {
            float real = argc > 7 ? strtof(argv[7], NULL) : DEFAULT_LIGHT_R;
            float imag = argc > 8 ? strtof(argv[8], NULL) : DEFAULT_LIGHT_I;

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