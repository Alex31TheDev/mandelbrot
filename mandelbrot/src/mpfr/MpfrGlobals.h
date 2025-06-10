#pragma once
#ifdef USE_MPFR

#include "mpreal.h"

#include "../scalar/ScalarGlobals.h"

namespace MpfrGlobals {
    using namespace mpfr;
    using namespace ScalarGlobals;

    constexpr int digits = 50;

    const mpreal bailout_mp = BAILOUT;

    extern mpreal aspect_mp;
    extern mpreal halfWidth_mp, halfHeight_mp;
    extern mpreal invWidth_mp, invHeight_mp;
    extern mpreal realScale_mp, imagScale_mp;

    extern mpreal point_r_mp, point_i_mp;
    extern mpreal seed_r_mp, seed_i_mp;

    void initMpfr(int prec = 0);
    void initMpfrValues(const char *pr_str, const char *pi_str);
}

#else

namespace MpfrGlobals {
    [[maybe_unused]] static void initMpfr(int = 0) {}
    [[maybe_unused]] static void initMpfrValues(const char *, const char *) {}
}

#endif