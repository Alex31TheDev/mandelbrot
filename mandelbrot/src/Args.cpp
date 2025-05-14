#include <cstdlib>
#include <cstdio>
#include <cstring>
#define _USE_MATH_DEFINES
#include <cmath>

#include "Args.h"

#include "GlobalsScalar.h"
#ifdef __AVX2__
#include "GlobalsVector.h"
#endif

struct ColorMethod {
    const char *name;
    int id;
};

const ColorMethod COLOR_METHODS[] = {
   { "smooth_iterations", 0 },
   { "light", 1 },
   { NULL, -1 }
};

const char usage[] =
"Usage: ./mandelbrot width height point_r point_i scale "
"[colorMethod] [freq_r / light_r] [freq_g / light_i] [freq_b] [freqMult]";

void setArgs_vec() {
#ifdef __AVX2__
    f_invCount_vec = _mm_set1_ps(invCount);

    f_freq_r_vec = _mm_set1_ps(freq_r);
    f_freq_g_vec = _mm_set1_ps(freq_g);
    f_freq_b_vec = _mm_set1_ps(freq_b);

    f_light_r_vec = _mm_set1_ps(light_r);
    f_light_i_vec = _mm_set1_ps(light_i);
    f_light_h_vec = _mm_set1_ps(light_h);
#endif
}

bool parseArgs(int argc, char **argv) {
    if (argc < 6 || argc > 11) {
        fprintf(stderr, "Invalid args.\n%s\n", usage);
        return false;
    }

    width = strtol(argv[1], NULL, 10);
    height = strtol(argv[2], NULL, 10);

    if (width <= 0 || height <= 0) {
        fprintf(stderr, "Invalid args.\nWidth and height must be > 0.\n");
        return false;
    }

    half_w = (double)width * 0.5;
    half_h = (double)height * 0.5;

    point_r = strtod(argv[3], NULL);
    point_i = strtod(argv[4], NULL);

    scale = strtod(argv[5], NULL);

    if (scale <= 0.0) {
        fprintf(stderr, "Invalid args.\nScale must be > 0.\n");
        return false;
    }

    count = (int)(pow(1.3, scale) + 22.0 * scale);
    invCount = 1.0f / count;
    scale = 1.0 / pow(2.0, scale);

    if (argc > 6) {
        int found = 0;

        for (const ColorMethod *method = COLOR_METHODS; method->name; method++) {
            if (strcmp(argv[6], method->name) == 0) {
                colorMethod = method->id;
                found = 1;

                break;
            }
        }

        if (!found) {
            fprintf(stderr, "Invalid colorMethod '%s'. Valid options: ", argv[6]);

            for (const ColorMethod *method = COLOR_METHODS; method->name; method++) {
                fprintf(stderr, "%s%s", method->name, (method + 1)->name ? ", " : "\n");
            }

            return false;
        }
    } else {
        colorMethod = COLOR_METHODS[0].id;
    }

    switch (colorMethod) {
        case 0:
        {
            freq_r = (argc > 7 ? strtof(argv[7], NULL) : DEFAULT_FREQ_R);
            freq_g = (argc > 8 ? strtof(argv[8], NULL) : DEFAULT_FREQ_G);
            freq_b = (argc > 9 ? strtof(argv[9], NULL) : DEFAULT_FREQ_B);
            freqMult = (argc > 10 ? strtof(argv[9], NULL) : DEFAULT_FREQ_MULT);

            freq_r *= freqMult;
            freq_g *= freqMult;
            freq_b *= freqMult;
        }
        break;

        case 1:
        {
            light_r = (argc > 7 ? strtof(argv[7], NULL) : DEFAULT_LIGHT_R);
            light_i = (argc > 8 ? strtof(argv[8], NULL) : DEFAULT_LIGHT_I);

            light_h = sqrtf(light_r * light_r + light_i * light_i);

            if (light_h <= 0) {
                fprintf(stderr, "Invalid args.\nLight vector must be non-zero.\n");
                return false;
            }

            light_r /= light_h;
            light_i /= light_h;
        }
        break;
    }

    setArgs_vec();
    return true;
}