#pragma once
#ifdef USE_VECTORS
#include "CommonDefs.h"

#include <cstdint>

#include "VectorTypes.h"
#include "../scalar/ScalarTypes.h"

#include "../util/InlineUtil.h"

namespace VectorRenderer {
    void VECTOR_CALL initCoordsSIMD(simd_full_t &cr, simd_full_t &ci,
        simd_full_t &zr, simd_full_t &zi,
        simd_full_t &dr, simd_full_t &di);
    simd_full_t VECTOR_CALL iterateFractalSIMD(
        simd_full_t cr, simd_full_t ci,
        simd_full_t &zr, simd_full_t &zi,
        simd_full_t &dr, simd_full_t &di,
        simd_full_t &mag, simd_full_mask_t &active
    );

    simd_half_int_t VECTOR_CALL colorToIntSIMD(simd_half_t val);
    void VECTOR_CALL setPixelsSIMD(uint8_t *pixels, size_t &pos, int width,
        simd_half_t R, simd_half_t G, simd_half_t B);
    void VECTOR_CALL colorPixelsSIMD(uint8_t *pixels, size_t &pos, int width,
        simd_full_t iter, simd_full_t mag,
        simd_full_mask_t active,
        simd_full_t zr, simd_full_t zi,
        simd_full_t dr, simd_full_t di);

    void VECTOR_CALL renderPixelSIMD(uint8_t *pixels, size_t &pos, int width,
        scalar_full_t x, simd_full_t ci, uint64_t *totalIterCount = nullptr);
}

#endif