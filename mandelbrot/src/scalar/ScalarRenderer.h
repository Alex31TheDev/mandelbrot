#pragma once
#include "CommonDefs.h"

#include <cstdint>

#include "ScalarTypes.h"

#if defined(USE_SCALAR)
#include "../render/RenderIterationStats.h"

namespace ScalarRenderer {
    void initCoordsScalar(scalar_full_t &cr, scalar_full_t &ci,
        scalar_full_t &zr, scalar_full_t &zi,
        scalar_full_t &dr, scalar_full_t &di);
    int iterateFractalScalar(scalar_full_t cr, scalar_full_t ci,
        scalar_full_t &zr, scalar_full_t &zi,
        scalar_full_t &dr, scalar_full_t &di,
        scalar_full_t &mag);

    void sampleColorFormula(scalar_half_t val,
        scalar_half_t &outR, scalar_half_t &outG, scalar_half_t &outB);

    void setPixel(uint8_t *pixels, size_t &pos,
        scalar_half_t R, scalar_half_t G, scalar_half_t B);
    void colorPixelScalar(uint8_t *pixels, size_t &pos,
        int i, scalar_full_t mag,
        scalar_full_t zr, scalar_full_t zi,
        scalar_full_t dr, scalar_full_t di);

    void renderPixelScalar(uint8_t *pixels, size_t &pos,
        int x, scalar_full_t ci,
        OptionalIterationStats iterStats = std::nullopt,
        std::optional<int> y = std::nullopt);
}

#else

namespace ScalarRenderer {
    void sampleColorFormula(scalar_half_t val,
        scalar_half_t &outR, scalar_half_t &outG, scalar_half_t &outB);

    void setPixel(uint8_t *pixels, size_t &pos,
        scalar_half_t R, scalar_half_t G, scalar_half_t B);

#if USE_SCALAR_COLORING
    void colorPixelScalar(uint8_t *pixels, size_t &pos,
        int i, scalar_full_t mag,
        scalar_full_t zr, scalar_full_t zi,
        scalar_full_t dr, scalar_full_t di);
#endif
}

#endif