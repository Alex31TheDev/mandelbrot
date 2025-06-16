#ifdef USE_MPFR
#include "MPFRRenderer.h"

#include <cstdint>

#include "mpreal.h"
#include "../scalar/ScalarTypes.h"

#include "MPFRGlobals.h"
#include "../scalar/ScalarGlobals.h"
using namespace MPFRGlobals;
using namespace ScalarGlobals;

#include "MPFRCoords.h"

#define FORMULA_MPFR
#include "../formulas/FractalFormulas.h"

#include "../scalar/ScalarRenderer.h"
using namespace ScalarRenderer;

#include "../util/InlineUtil.h"

static FORCE_INLINE void complexInverse_mp(mpreal &cr, mpreal &ci) {
    const mpreal cmag = cr * cr + ci * ci;

    if (cmag != 0) {
        cr = cr / cmag;
        ci = -ci / cmag;
    }
}

namespace MPFRRenderer {
    FORCE_INLINE void initCoords_mp(
        mpreal &cr, mpreal &ci,
        mpreal &zr, mpreal &zi,
        mpreal &dr, mpreal &di
    ) {
        if (isInverse) {
            complexInverse_mp(cr, ci);
        }

        if (isJuliaSet) {
            zr = cr;
            zi = ci;

            cr = seed_r_mp;
            ci = seed_i_mp;
        } else {
            zr = seed_r_mp;
            zi = seed_i_mp;
        }

        dr = 1;
        di = 0;
    }

    FORCE_INLINE int iterateFractalMPFR(
        const mpreal &cr, const mpreal &ci,
        mpreal &zr, mpreal &zi,
        mpreal &dr, mpreal &di,
        mpreal &mag
    ) {
        mag = 0;
        int i = 0;

        for (; i < count; i++) {
            const mpreal zr2 = zr * zr;
            const mpreal zi2 = zi * zi;
            mag = zr2 + zi2;

            if (mag > BAILOUT) break;

            switch (colorMethod) {
                case 2:
                    derivative(zr, zi, dr, di, mag, dr, di);
                    break;
            }

            formula(cr, ci, zr, zi, zr2, zi2, mag, zr, zi);
        }

        return i;
    }

    FORCE_INLINE void colorPixelMPFR(
        uint8_t *pixels, size_t &pos,
        int i, const mpreal &mag,
        const mpreal &zr, const mpreal &zi,
        const mpreal &dr, const mpreal &di
    ) {
        const scalar_half_t mag_sc = CAST_H(mag);

        const scalar_half_t zr_sc = CAST_H(zr);
        const scalar_half_t zi_sc = CAST_H(zi);

        const scalar_half_t dr_sc = CAST_H(dr);
        const scalar_half_t di_sc = CAST_H(di);

        colorPixelScalar(pixels, pos, i, mag_sc, zr_sc, zi_sc, dr_sc, di_sc);
    }

    void renderPixelMPFR(
        uint8_t *pixels, size_t &pos,
        int x, mpreal ci
    ) {
        mpreal cr = getCenterReal_mp(x);

        mpreal zr, zi;
        mpreal dr, di;

        initCoords_mp(cr, ci, zr, zi, dr, di);

        mpreal mag;
        int i = iterateFractalMPFR(cr, ci, zr, zi, dr, di, mag);

        colorPixelMPFR(pixels, pos, i, mag, zr, zi, dr, di);
    }
}

#endif