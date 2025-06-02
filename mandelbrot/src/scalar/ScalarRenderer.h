#pragma once

#include <cstdint>

#include "ScalarTypes.h"

namespace ScalarRenderer {
    static int iterateFractalScalar(scalar_full_t cr, scalar_full_t ci,
        scalar_full_t &zr, scalar_full_t &zi,
        scalar_full_t &dr, scalar_full_t &di,
        scalar_full_t &mag);

    uint8_t pixelToInt(scalar_half_t val);
    inline void setPixel(uint8_t *pixels, int &pos,
        scalar_half_t R, scalar_half_t G, scalar_half_t B);
    void colorPixelScalar(uint8_t *pixels, int &pos,
        int i, scalar_full_t mag,
        scalar_full_t zr, scalar_full_t zi,
        scalar_full_t dr, scalar_full_t di);

    void renderPixelScalar(uint8_t *pixels, int &pos,
        int x, scalar_full_t ci);
}
