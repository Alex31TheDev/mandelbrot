#ifdef USE_MPFR
#include "MpfrGlobals.h"

#include "MpfrTypes.h"

#include "../scalar/ScalarGlobals.h"
#include "../render/RenderGlobals.h"
using namespace ScalarGlobals;
using namespace RenderGlobals;

namespace MpfrGlobals {
    mpfr_t bailout_mp, aspect_mp;
    mpfr_t halfWidth_mp, halfHeight_mp;
    mpfr_t invWidth_mp, invHeight_mp;
    mpfr_t realScale_mp, imagScale_mp;

    mpfr_t point_r_mp, point_i_mp;
    mpfr_t seed_r_mp, seed_i_mp;

    void initMpfrValues(const char *pr_str, const char *pi_str) {
        init_mpfr_pool();

        init_set(bailout_mp, BAILOUT);
        create_copy(aspect_mp, div(init_set_res(width), init_set_res(height)));

        create_copy(halfWidth_mp, div(init_set_res(width), init_set_res(2)));
        create_copy(halfHeight_mp, div(init_set_res(height), init_set_res(2)));

        mpfr_init2(invWidth_mp, PRECISION);
        mpfr_init2(invHeight_mp, PRECISION);
        mpfr_ui_div(invWidth_mp, 1, *init_set_res(width), ROUNDING);
        mpfr_ui_div(invHeight_mp, 1, *init_set_res(height), ROUNDING);

        mpfr_t zoomPow;
        create_copy(zoomPow, pow(init_set_res(10), init_set_res(zoom)));

        create_copy(realScale_mp, div(init_set_res(1), &zoomPow));
        create_copy(imagScale_mp, div(&realScale_mp, &aspect_mp));

        mpfr_clear(zoomPow);

        mpfr_strtofr(point_r_mp, pr_str, nullptr, 10, ROUNDING);
        mpfr_strtofr(point_i_mp, pi_str, nullptr, 10, ROUNDING);

        init_set(seed_r_mp, seed_r);
        init_set(seed_i_mp, seed_i);
    }
}

#endif