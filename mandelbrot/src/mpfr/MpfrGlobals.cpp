#ifdef USE_MPFR
#include "MpfrGlobals.h"

#include "mpreal.h"
using namespace mpfr;

#include "../scalar/ScalarGlobals.h"
using namespace ScalarGlobals;

namespace MpfrGlobals {
    mpreal aspect_mp;
    mpreal halfWidth_mp, halfHeight_mp;
    mpreal invWidth_mp, invHeight_mp;
    mpreal realScale_mp, imagScale_mp;

    mpreal point_r_mp = 0, point_i_mp = 0;
    mpreal seed_r_mp = 0, seed_i_mp = 0;

    void initMpfr() {
        mpreal::set_default_prec(digits2bits(digits));
    }

    void initGlobals() {
        aspect_mp = static_cast<mpreal>(width) / height;

        halfWidth_mp = static_cast<mpreal>(width) / 2;
        halfHeight_mp = static_cast<mpreal>(height) / 2;

        invWidth_mp = 1 / static_cast<mpreal>(width);
        invHeight_mp = 1 / static_cast<mpreal>(height);

        mpreal zoomPow = powr(10, zoom);

        realScale_mp = 1 / zoomPow;
        imagScale_mp = realScale_mp / aspect_mp;
    }
}

#endif