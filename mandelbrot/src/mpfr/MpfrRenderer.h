#pragma once
#ifdef USE_MPFR

#include <cstdint>

#include "mpreal.h"

namespace MpfrRenderer {
    using namespace mpfr;

    int iterateFractalMpfr(mpreal cr, mpreal ci,
        mpreal &zr, mpreal &zi,
        mpreal &dr, mpreal &di,
        mpreal &mag);

    void colorPixelMpfr(uint8_t *pixels, int &pos,
        int i, mpreal mag,
        mpreal zr, mpreal zi,
        mpreal dr, mpreal di);

    void renderPixelMpfr(uint8_t *pixels, int &pos,
        int x, mpfr::mpreal ci);
}

#endif
