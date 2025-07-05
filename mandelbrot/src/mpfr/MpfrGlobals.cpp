#ifdef USE_MPFR
#include "CommonDefs.h"
#include "MPFRGlobals.h"

#include "MpfrTypes.h"

#include "../scalar/ScalarGlobals.h"
#include "../render/RenderGlobals.h"
using namespace ScalarGlobals;
using namespace RenderGlobals;

namespace MPFRGlobals {
    mpfr_number_t aspect_mp;
    mpfr_number_t halfWidth_mp, halfHeight_mp;
    mpfr_number_t invWidth_mp, invHeight_mp;
    mpfr_number_t realScale_mp, imagScale_mp;

    mpfr_number_t point_r_mp = 0.0, point_i_mp = 0.0;
    mpfr_number_t seed_r_mp = 0.0, seed_i_mp = 0.0;

    void initMPFR(int prec) {
        if (prec <= 0) prec = digits;
        mpreal::set_default_prec(digits2bits(prec));
    }

    void initMPFRValues(const char *pr_str, const char *pi_str) {
        aspect_mp = static_cast<mpfr_number_t>(width) / height;

        halfWidth_mp = static_cast<mpfr_number_t>(width) / 2.0;
        halfHeight_mp = static_cast<mpfr_number_t>(height) / 2.0;

        invWidth_mp = 1.0 / static_cast<mpfr_number_t>(width);
        invHeight_mp = 1.0 / static_cast<mpfr_number_t>(height);

        const mpfr_number_t zoomPow = powr(10.0, zoom);

        realScale_mp = 1.0 / zoomPow;
        imagScale_mp = realScale_mp / aspect_mp;

        point_r_mp = mpfr_number_t(pr_str);
        point_i_mp = mpfr_number_t(pi_str);

        seed_r_mp = seed_r;
        seed_i_mp = seed_i;
    }
}

#endif