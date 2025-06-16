#pragma once
#ifdef USE_MPFR

#include <cstdint>

#include "mpreal.h"

#include "../util/InlineUtil.h"

namespace MPFRRenderer {
    using namespace mpfr;

    void initCoords_mp(mpreal &cr, mpreal &ci,
        mpreal &zr, mpreal &zi,
        mpreal &dr, mpreal &di);
    int iterateFractalMPFR(const mpreal &cr, const mpreal &ci,
        mpreal &zr, mpreal &zi,
        mpreal &dr, mpreal &di,
        mpreal &mag);

    void colorPixelMPFR(uint8_t *pixels, size_t &pos,
        int i, const mpreal &mag,
        const mpreal &zr, const mpreal &zi,
        const mpreal &dr, const mpreal &di);

    void renderPixelMPFR(uint8_t *pixels, size_t &pos,
        int x, mpreal ci);
}

#endif
