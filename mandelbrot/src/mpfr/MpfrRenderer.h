#pragma once
#ifdef USE_MPFR
#include "CommonDefs.h"

#include <cstdint>

#include "MpfrTypes.h"

#include "../util/InlineUtil.h"

namespace MPFRRenderer {
    void initCoordsMPFR(mpfr_number_t &cr, mpfr_number_t &ci,
        mpfr_number_t &zr, mpfr_number_t &zi,
        mpfr_number_t &dr, mpfr_number_t &di);
    int iterateFractalMPFR(mpfr_param_t cr, mpfr_param_t ci,
        mpfr_number_t &zr, mpfr_number_t &zi,
        mpfr_number_t &dr, mpfr_number_t &di,
        mpfr_number_t &mag);

    void colorPixelMPFR(uint8_t *pixels, size_t &pos,
        int i, mpfr_param_t mag,
        mpfr_param_t zr, mpfr_param_t zi,
        mpfr_param_t dr, mpfr_param_t di);

    void renderPixelMPFR(uint8_t *pixels, size_t &pos,
        int x, mpfr_number_t ci, uint64_t *totalIterCount = nullptr);
}

#endif
