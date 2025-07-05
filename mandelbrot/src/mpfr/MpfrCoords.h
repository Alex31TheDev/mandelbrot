#pragma once
#ifdef USE_MPFR
#include "CommonDefs.h"

#include "MpfrTypes.h"
#include "MPFRGlobals.h"

#include "../util/InlineUtil.h"

FORCE_INLINE mpfr_number_t getCenterReal_mp(int x) {
    using namespace MPFRGlobals;
    return (x - halfWidth_mp) * invWidth_mp * realScale_mp + point_r_mp;
}

FORCE_INLINE mpfr_number_t getCenterImag_mp(int y) {
    using namespace MPFRGlobals;
    return (y - halfHeight_mp) * invHeight_mp * imagScale_mp - point_i_mp;
}

#endif
