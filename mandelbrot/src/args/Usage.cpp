#include "Usage.h"

#include <cstdio>

const char *usage =
"width height point_r point_i zoom "
"[count = auto] [useThreads = false] "
"[colorMethod = smooth_iterations] "
"[isJuliaSet = false] [isInverse = false] "
"[seed_r = 0] [seed_i = 0] [N = 2] "
"[freq_r/light_r] [freq_g/light_i] [freq_b] [freqMult]";

DEFINE_RANGE_ARRAY(char *, helpOptions, "-h", "--help", "help");

const char *replOption = "replio";
const char *exitOption = "exit";

void printUsage(const char *progPath, bool error) {
    FILE *out = error ? stderr : stdout;

    if (!progPath) {
        fprintf(out, "Usage: %s\n", usage);
        return;
    }

    const char *name = progPath;
    char sep = '/';

    for (int i = 0; progPath[i]; i++) {
        const char chr = progPath[i];

        if (chr == '/' || chr == '\\') {
            name = progPath + i + 1;
            sep = chr;
        }
    }

    fprintf(out, "Usage: .%c%s %s\n", sep, name, usage);
}