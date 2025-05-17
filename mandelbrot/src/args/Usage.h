#pragma once

#include <cstdio>

const char usage[] =
"Usage: ./mandelbrot width height point_r point_i scale "
"[colorMethod] [freq_r/light_r] [freq_g/light_i] [freq_b] [freqMult]";

void printUsage(bool error = false) {
    fprintf(error ? stderr : stdout, usage);
}
