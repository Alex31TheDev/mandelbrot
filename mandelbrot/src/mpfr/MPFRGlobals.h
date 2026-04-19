#pragma once
#ifdef USE_MPFR
#include "CommonDefs.h"

#include "MPFRTypes.h"

#include "../scalar/ScalarGlobals.h"

namespace MPFRGlobals {
    constexpr int digits = 50;

    extern mpfr_t bailout_mp;

    extern mpfr_t aspect_mp;
    extern mpfr_t halfWidth_mp, halfHeight_mp;
    extern mpfr_t invWidth_mp, invHeight_mp;
    extern mpfr_t realScale_mp, imagScale_mp;

    extern mpfr_t point_r_mp, point_i_mp;
    extern mpfr_t seed_r_mp, seed_i_mp;

    void initMPFR(int prec = 0);
    void initImageValues();
    void initMPFRValues(const char *pr_str, const char *pi_str);
}

#else

namespace MPFRGlobals {
    [[maybe_unused]] static void initMPFR(int = 0) {}
    [[maybe_unused]] static void initImageValues() {}
    [[maybe_unused]] static void initMPFRValues(const char *, const char *) {}
}

#endif
