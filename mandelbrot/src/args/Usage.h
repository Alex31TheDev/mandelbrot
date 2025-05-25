#pragma once

#include <cstdio>

#define MIN_ARGS 5
#define MAX_ARGS 16

const char usage[] =
"Usage: ./mandelbrot width height point_r point_i zoom "
"[count = auto] [useThreads = false]"
"[colorMethod = smooth_iterations] "
"[isJuliaSet = false] [isInverse = false] [seed_r = 0] [seed_i = 0] "
"[freq_r/light_r] [freq_g/light_i] [freq_b] [freqMult]";

void printUsage(bool error = false) {
    fprintf(error ? stderr : stdout, usage);
}
