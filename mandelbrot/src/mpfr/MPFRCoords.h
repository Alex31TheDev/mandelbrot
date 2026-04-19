#pragma once
#ifdef USE_MPFR
#include "CommonDefs.h"

#include "MPFRTypes.h"
#include "MPFRGlobals.h"

#include "util/InlineUtil.h"

FORCE_INLINE void getCenterReal_mp(mpfr_t out, int x) {
    using namespace MPFRGlobals;

    mpfr_set_si(out, x, MPFRTypes::ROUNDING);
    mpfr_sub(out, out, halfWidth_mp, MPFRTypes::ROUNDING);
    mpfr_mul(out, out, invWidth_mp, MPFRTypes::ROUNDING);
    mpfr_mul(out, out, realScale_mp, MPFRTypes::ROUNDING);
    mpfr_add(out, out, point_r_mp, MPFRTypes::ROUNDING);
}

FORCE_INLINE void getCenterImag_mp(mpfr_t out, int y) {
    using namespace MPFRGlobals;

    mpfr_set_si(out, y, MPFRTypes::ROUNDING);
    mpfr_sub(out, out, halfHeight_mp, MPFRTypes::ROUNDING);
    mpfr_mul(out, out, invHeight_mp, MPFRTypes::ROUNDING);
    mpfr_mul(out, out, imagScale_mp, MPFRTypes::ROUNDING);
    mpfr_sub(out, out, point_i_mp, MPFRTypes::ROUNDING);
}

#endif
