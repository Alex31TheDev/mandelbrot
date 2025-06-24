#pragma once
#ifdef USE_VECTORS
#include "CommonDefs.h"

#include <cstdint>

#include "VectorTypes.h"
#include "../scalar/ScalarTypes.h"

#include "../util/InlineUtil.h"

namespace VectorRenderer {
    void VECTOR_CALL initCoords_vec(simd_full_t &cr, simd_full_t &ci,
        simd_full_t &zr, simd_full_t &zi,
        simd_full_t &dr, simd_full_t &di);
    simd_full_t VECTOR_CALL iterateFractalSIMD(
        const simd_full_t &cr, const simd_full_t &ci,
        simd_full_t &zr, simd_full_t &zi,
        simd_full_t &dr, simd_full_t &di,
        simd_full_t &mag, simd_full_mask_t &active
    );

    simd_half_int_t VECTOR_CALL colorToInt_vec(const simd_half_t &val);
    void VECTOR_CALL setPixels_vec(uint8_t *pixels, size_t &pos, int width,
        const simd_half_t &R, const simd_half_t &G, const simd_half_t &B);
    void VECTOR_CALL colorPixelsSIMD(uint8_t *pixels, size_t &pos, int width,
        const simd_full_t &iter, const simd_full_t &mag,
        const simd_full_mask_t &active,
        const simd_full_t &zr, const simd_full_t &zi,
        const simd_full_t &dr, const simd_full_t &di);

    void VECTOR_CALL renderPixelSIMD(uint8_t *pixels, size_t &pos, int width,
        int x, scalar_full_t ci);
}

#endif