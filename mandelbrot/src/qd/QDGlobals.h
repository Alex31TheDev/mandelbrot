#pragma once
#ifdef USE_QD
#include "CommonDefs.h"

#include "QDTypes.h"

#include "scalar/ScalarGlobals.h"

namespace QDGlobals {
    using namespace ScalarGlobals;

    const qd_number_t bailout_qd = BAILOUT;

    extern qd_number_t aspect_qd;
    extern qd_number_t halfWidth_qd, halfHeight_qd;
    extern qd_number_t invWidth_qd, invHeight_qd;
    extern qd_number_t realScale_qd, imagScale_qd;
    extern qd_number_t zoom_qd;

    extern qd_number_t point_r_qd, point_i_qd;
    extern qd_number_t seed_r_qd, seed_i_qd;

    void initQD();
    void initImageValues();
    void initQDValues(const char *pr_str, const char *pi_str,
        const char *zoomStr, const char *sr_str, const char *si_str);
}

#else

namespace QDGlobals {
    [[maybe_unused]] static void initQD() {}
    [[maybe_unused]] static void initImageValues() {}
    [[maybe_unused]] static void initQDValues(
        const char *, const char *, const char *, const char *, const char *) {}
}

#endif
