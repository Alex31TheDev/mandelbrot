#ifdef USE_MPFR
#include "MpfrCoords.h"

#include "mpreal.h"

#include "MpfrGlobals.h"
using namespace MpfrGlobals;

mpfr::mpreal getCenterReal_mp(int x) {
    return (x - halfWidth_mp) * invWidth_mp * realScale_mp + point_r_mp;
}

mpfr::mpreal getCenterImag_mp(int y) {
    return(y - halfHeight_mp) * invHeight_mp * imagScale_mp - point_i_mp;
}

#endif