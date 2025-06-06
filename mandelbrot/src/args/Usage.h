#pragma once

#include <cstdio>

#define MIN_ARGS 5
#define MAX_ARGS 17

static const char streamArgName[] = "stream_io";

static const char usage[] =
"width height point_r point_i zoom "
"[count = auto] [useThreads = false]"
"[colorMethod = smooth_iterations] "
"[isJuliaSet = false] [isInverse = false] "
"[seed_r = 0] [seed_i = 0] [N = 2] "
"[freq_r/light_r] [freq_g/light_i] [freq_b] [freqMult]";

static void printUsage(const char *progPath = nullptr, bool error = false) {
    FILE *out = error ? stderr : stdout;

    if (!progPath) {
        fprintf(out, "Usage: %s\n", usage);
        return;
    }

    const char *name = progPath;
    char sep = '/';

    for (int i = 0; progPath[i]; i++) {
        char chr = progPath[i];

        if (chr == '/' || chr == '\\') {
            name = progPath + i + 1;
            sep = chr;
        }
    }

    fprintf(out, "Usage: .%c%s %s\n", sep, name, usage);
}
