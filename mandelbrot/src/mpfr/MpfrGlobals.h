#pragma once
#ifdef USE_MPFR

#include <mpfr.h>

namespace MpfrGlobals {
    extern mpfr_t bailout_mp, aspect_mp;
    extern mpfr_t halfWidth_mp, halfHeight_mp;
    extern mpfr_t invWidth_mp, invHeight_mp;
    extern mpfr_t realScale_mp, imagScale_mp;

    extern mpfr_t point_r_mp, point_i_mp;
    extern mpfr_t seed_r_mp, seed_i_mp;

    void initMpfrValues(const char *pr_str, const char *pi_str);
}

#else

namespace MpfrGlobals {
    [[maybe_unused]] static void initMpfrValues(const char *, const char *) {}
}

#endif