#pragma once
#ifdef USE_MPFR
#include "CommonDefs.h"

#include "MpfrTypes.h"

#include "../scalar/ScalarGlobals.h"

namespace MPFRGlobals {
    using namespace mpfr;
    using namespace ScalarGlobals;

    constexpr int digits = 50;

    const mpfr_number_t bailout_mp = BAILOUT;

    extern mpfr_number_t aspect_mp;
    extern mpfr_number_t halfWidth_mp, halfHeight_mp;
    extern mpfr_number_t invWidth_mp, invHeight_mp;
    extern mpfr_number_t realScale_mp, imagScale_mp;

    extern mpfr_number_t point_r_mp, point_i_mp;
    extern mpfr_number_t seed_r_mp, seed_i_mp;

    void initMPFR(int prec = 0);
    void initMPFRValues(const char *pr_str, const char *pi_str);
}

#else

namespace MPFRGlobals {
    [[maybe_unused]] static void initMPFR(int = 0) {}
    [[maybe_unused]] static void initMPFRValues(const char *, const char *) {}
}

#endif