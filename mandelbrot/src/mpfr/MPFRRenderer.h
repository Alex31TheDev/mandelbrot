#pragma once
#ifdef USE_MPFR
#include "CommonDefs.h"

#include <cstdint>

#include "MPFRTypes.h"
#include "../render/RenderIterationStats.h"


namespace MPFRRenderer {
    void initCoordsMPFR(mpfr_t cr, mpfr_t ci,
        mpfr_t zr, mpfr_t zi,
        mpfr_t dr, mpfr_t di);
    int iterateFractalMPFR(mpfr_srcptr cr, mpfr_srcptr ci,
        mpfr_t zr, mpfr_t zi,
        mpfr_t dr, mpfr_t di,
        mpfr_t mag);

    void colorPixelMPFR(uint8_t *pixels, size_t &pos,
        int i, mpfr_srcptr mag,
        mpfr_srcptr zr, mpfr_srcptr zi,
        mpfr_srcptr dr, mpfr_srcptr di);

    void renderPixelMPFR(uint8_t *pixels, size_t &pos,
        int x, mpfr_srcptr ci,
        OptionalIterationStats iterStats = std::nullopt,
        std::optional<int> y = std::nullopt);
}

#endif
