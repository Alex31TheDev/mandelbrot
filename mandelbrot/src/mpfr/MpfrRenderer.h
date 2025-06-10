#pragma once
#ifdef USE_MPFR

#include <cstdint>

#include <mpfr.h>

#include "../util/InlineUtil.h"

namespace MpfrRenderer {
    FORCE_INLINE void initCoords_mp(mpfr_t &cr, mpfr_t &ci,
        mpfr_t &zr, mpfr_t &zi);
    FORCE_INLINE int iterateFractalMpfr(const mpfr_t &cr, const mpfr_t &ci,
        mpfr_t &zr, mpfr_t &zi,
        mpfr_t &dr, mpfr_t &di,
        mpfr_t &mag);

    FORCE_INLINE void colorPixelMpfr(uint8_t *pixels, int &pos,
        int i, const mpfr_t &mag,
        const mpfr_t &zr, const mpfr_t &zi,
        const mpfr_t &dr, const mpfr_t &di);

    void renderPixelMpfr(uint8_t *pixels, int &pos,
        int x, mpfr_t *ci);
}

#endif
