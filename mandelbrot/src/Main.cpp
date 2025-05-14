#include "Args.h"
#include "Image.h"

#include "GlobalsScalar.h"
#ifdef __AVX2__
#include "Vector.h"
#include "GlobalsVector.h"
#else
#include "Scalar.h"
#endif

const char filename[] = "mandelbrot.png";

void renderImage() {
    int pos = 0;

    for (int y = 0; y < height; y++) {
        double ci = (y - half_h) * scale - point_i;

#ifdef __AVX2__
        for (int x = 0; x < width; x += SIMD_WIDTH) {
            int pixels_left = width - x;
            int simd_width = (pixels_left < SIMD_WIDTH ? pixels_left : SIMD_WIDTH);

            renderPixelSimd(pixels, pos, simd_width, x, ci);
        }
#else
        for (int x = 0; x < width; x++) {
            renderPixelScalar(pixels, pos, x, ci);
        }
#endif
    }
}

int main(int argc, char **argv) {
    if (!parseArgs(argc, argv)) return 1;
    if (!allocPixels(width, height)) return 1;

    renderImage();

    if (!saveImage(filename)) return 1;
    return 0;
}