#pragma once
#ifdef USE_MPFR

#include "mpreal.h"

#include "MpfrGlobals.h"

#include "../util/InlineUtil.h"

FORCE_INLINE mpfr::mpreal getCenterReal_mp(int x) {
    using namespace MpfrGlobals;
    return (x - halfWidth_mp) * invWidth_mp * realScale_mp + point_r_mp;
}

FORCE_INLINE mpfr::mpreal getCenterImag_mp(int y) {
    using namespace MpfrGlobals;
    return (y - halfHeight_mp) * invHeight_mp * imagScale_mp - point_i_mp;
}

#endif
