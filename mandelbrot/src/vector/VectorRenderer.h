#pragma once
#ifdef USE_VECTORS

#include "../scalar/ScalarTypes.h"
#include "VectorTypes.h"

#include <cstdint>
#include <xmmintrin.h>

namespace VectorRenderer {
    inline simd_half_int_t pixelToInt_vec(simd_half_t val);
    inline void setPixels_vec(uint8_t *pixels, int &pos,
        int width, simd_half_t R, simd_half_t G, simd_half_t B);
    void renderPixelSimd(uint8_t *pixels, int &pos,
        int width, int x, scalar_full_t ci);
}

#endif