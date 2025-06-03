#pragma once
#ifdef USE_VECTORS

#include "VectorTypes.h"

#include <cstdint>

#include <xmmintrin.h>

#include "../scalar/ScalarTypes.h"

namespace VectorRenderer {
    const simd_full_t iterateFractalSimd(const simd_full_t &cr, const simd_full_t &ci,
        simd_full_t &zr, simd_full_t &zi,
        simd_full_t &dr, simd_full_t &di,
        simd_full_t &mag, simd_full_mask_t &active);

    const simd_half_int_t pixelToInt_vec(const simd_half_t &val);
    inline void setPixels_vec(uint8_t *pixels, int &pos,
        const int width, const simd_half_t &R, const simd_half_t &G, const simd_half_t &B);
    void colorPixelsSimd(uint8_t *pixels, int &pos, const int width,
        const simd_full_t &iter, const simd_full_t &mag, const simd_full_mask_t &active,
        const simd_full_t &zr, const simd_full_t &zi,
        const simd_full_t &dr, const simd_full_t &di);

    void renderPixelSimd(uint8_t *pixels, int &pos,
        const int width, const int x, scalar_full_t ci);
}

#endif