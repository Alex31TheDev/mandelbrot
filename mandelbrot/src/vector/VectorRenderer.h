#pragma once
#ifdef USE_VECTORS

#include "VectorTypes.h"

#include <cstdint>

#include <xmmintrin.h>

#include "../scalar/ScalarTypes.h"

namespace VectorRenderer {
    static simd_full_t iterateFractalSimd(simd_full_t cr, simd_full_t ci,
        simd_full_t &zr, simd_full_t &zi,
        simd_full_t &dr, simd_full_t &di,
        simd_full_t &mag, simd_full_mask_t &active);

    simd_half_int_t pixelToInt_vec(simd_half_t val);
    inline void setPixels_vec(uint8_t *pixels, int &pos,
        int width, simd_half_t R, simd_half_t G, simd_half_t B);
    void colorPixelsSimd(uint8_t *pixels, int &pos, int width,
        simd_full_t iter, simd_full_t mag, simd_full_mask_t active,
        simd_full_t zr, simd_full_t zi,
        simd_full_t dr, simd_full_t di);

    void renderPixelSimd(uint8_t *pixels, int &pos,
        int width, int x, scalar_full_t ci);
}

#endif