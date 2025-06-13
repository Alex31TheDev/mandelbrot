#include "Usage.h"

#include <cstdio>
#include <cstddef>

#define _DEFINE_STRING_ARRAY(name, ...) \
    const char* name[] = { __VA_ARGS__ }; \
    const size_t name##Size = sizeof(name) / sizeof(name[0]); \
    const char** name##Range::begin() const { return name; } \
    const char** name##Range::end() const { return name + name##Size; } \

const char *usage =
"width height point_r point_i zoom "
"[count = auto] [useThreads = false] "
"[colorMethod = smooth_iterations] "
"[isJuliaSet = false] [isInverse = false] "
"[seed_r = 0] [seed_i = 0] [N = 2] "
"[freq_r/light_r] [freq_g/light_i] [freq_b] [freqMult]";

_DEFINE_STRING_ARRAY(helpOptions, "-h", "--help", "help");

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