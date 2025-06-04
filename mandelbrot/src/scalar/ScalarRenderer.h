#pragma once

#include <cstdint>

#include "ScalarTypes.h"

#include "../util/InlineUtil.h"

namespace ScalarRenderer {
    FORCE_INLINE int iterateFractalScalar(const scalar_full_t cr, const scalar_full_t ci,
        scalar_full_t &zr, scalar_full_t &zi,
        scalar_full_t &dr, scalar_full_t &di,
        scalar_full_t &mag);

    FORCE_INLINE uint8_t pixelToInt(const scalar_half_t val);
    FORCE_INLINE void setPixel(uint8_t *pixels, int &pos,
        const scalar_half_t R, const  scalar_half_t G, const scalar_half_t B);
    FORCE_INLINE void colorPixelScalar(uint8_t *pixels, int &pos,
        const int i, const scalar_full_t mag,
        const scalar_full_t zr, const scalar_full_t zi,
        const scalar_full_t dr, const scalar_full_t di);

    void renderPixelScalar(uint8_t *pixels, int &pos,
        int x, scalar_full_t ci);
}
