#pragma once
#ifdef USE_MPFR

#include "MpfrTypes.h"

#include "MpfrGlobals.h"

#include "../util/InlineUtil.h"

FORCE_INLINE void getCenterReal_mp(mpfr_t &out, int x) {
    using namespace MpfrGlobals;
    create_copy(out, add(mul(mul(sub(init_set_res(x), &halfWidth_mp), &invWidth_mp), &realScale_mp), &point_r_mp));
}

FORCE_INLINE void getCenterImag_mp(mpfr_t &out, int y) {
    using namespace MpfrGlobals;
    create_copy(out, add(mul(mul(sub(init_set_res(y), &halfHeight_mp), &invWidth_mp), &imagScale_mp), &point_i_mp));
}

#endif
