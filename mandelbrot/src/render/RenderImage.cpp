#include "RenderImage.h"

#include "../image/Image.h"

#include "CenterCoords.h"
#ifdef __AVX2__
#include "../vector/VectorRenderer.h"
#include "../vector/VectorGlobals.h"

using namespace VectorRenderer;
using namespace VectorGlobals;
#else
#include "../scalar/ScalarRenderer.h"
using namespace ScalarRenderer;
#endif
#include "../scalar/ScalarGlobals.h"
using namespace ScalarGlobals;

void renderImage(Image &image) {
    int pos = 0;

    for (int y = 0; y < height; y++) {
        double ci = getCenterImag(y);

#ifdef __AVX2__
        for (int x = 0; x < width; x += SIMD_WIDTH) {
            int pixels_left = width - x;
            int simd_width = (pixels_left < SIMD_WIDTH ? pixels_left : SIMD_WIDTH);

            renderPixelSimd(image.pixels(), pos, simd_width, x, ci);
        }
#else
        for (int x = 0; x < width; x++) {
            renderPixelScalar(image.pixels(), pos, x, ci);
        }
#endif
    }
}