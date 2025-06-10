#pragma once
#ifdef USE_MPFR

#include <cstdint>

#include "mpreal.h"

#include "../util/InlineUtil.h"

namespace MpfrRenderer {
    using namespace mpfr;

    FORCE_INLINE void initCoords_mp(mpreal &cr, mpreal &ci,
        mpreal &zr, mpreal &zi);
    FORCE_INLINE int iterateFractalMpfr(const mpreal &cr, const mpreal &ci,
        mpreal &zr, mpreal &zi,
        mpreal &dr, mpreal &di,
        mpreal &mag);

    FORCE_INLINE void colorPixelMpfr(uint8_t *pixels, int &pos,
        int i, const mpreal &mag,
        const mpreal &zr, const mpreal &zi,
        const mpreal &dr, const mpreal &di);

    void renderPixelMpfr(uint8_t *pixels, int &pos,
        int x, mpreal ci);
}

#endif
