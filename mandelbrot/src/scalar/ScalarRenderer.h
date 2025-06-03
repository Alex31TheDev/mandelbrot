#pragma once

#include <cstdint>

#include "ScalarTypes.h"

namespace ScalarRenderer {
    const int iterateFractalScalar(const scalar_full_t cr, const scalar_full_t ci,
        scalar_full_t &zr, scalar_full_t &zi,
        scalar_full_t &dr, scalar_full_t &di,
        scalar_full_t &mag);

    const uint8_t pixelToInt(const scalar_half_t val);
    inline void setPixel(uint8_t *pixels, int &pos,
        const scalar_half_t R, const  scalar_half_t G, const scalar_half_t B);
    void colorPixelScalar(uint8_t *pixels, int &pos,
        const int i, const scalar_full_t mag,
        const scalar_full_t zr, const scalar_full_t zi,
        const scalar_full_t dr, const scalar_full_t di);

    void renderPixelScalar(uint8_t *pixels, int &pos,
        int x, scalar_full_t ci);
}
