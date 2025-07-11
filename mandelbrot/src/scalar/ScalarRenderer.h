#pragma once
#include "CommonDefs.h"

#include <cstdint>

#include "ScalarTypes.h"

#if defined(USE_SCALAR)

namespace ScalarRenderer {
    void initCoordsScalar(scalar_full_t &cr, scalar_full_t &ci,
        scalar_full_t &zr, scalar_full_t &zi,
        scalar_full_t &dr, scalar_full_t &di);
    int iterateFractalScalar(scalar_full_t cr, scalar_full_t ci,
        scalar_full_t &zr, scalar_full_t &zi,
        scalar_full_t &dr, scalar_full_t &di,
        scalar_full_t &mag);

    uint8_t colorToInt(scalar_half_t val);
    void setPixel(uint8_t *pixels, size_t &pos,
        scalar_half_t R, scalar_half_t G, scalar_half_t B);
    void colorPixelScalar(uint8_t *pixels, size_t &pos,
        int i, scalar_full_t mag,
        scalar_full_t zr, scalar_full_t zi,
        scalar_full_t dr, scalar_full_t di);

    void renderPixelScalar(uint8_t *pixels, size_t &pos,
        int x, scalar_full_t ci, uint64_t *totalIterCount = nullptr);
}

#else

namespace ScalarRenderer {
    uint8_t colorToInt(scalar_half_t val);
    void setPixel(uint8_t *pixels, size_t &pos,
        scalar_half_t R, scalar_half_t G, scalar_half_t B);

#if defined(USE_MPFR)
    void colorPixelScalar(uint8_t *pixels, size_t &pos,
        int i, scalar_full_t mag,
        scalar_full_t zr, scalar_full_t zi,
        scalar_full_t dr, scalar_full_t di);
#endif
}

#endif