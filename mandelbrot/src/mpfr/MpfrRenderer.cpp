#ifdef USE_MPFR
#include "MpfrRenderer.h"

#include <cstdint>

#include "MpfrTypes.h"

#include "MpfrGlobals.h"
#include "../scalar/ScalarGlobals.h"
using namespace MpfrGlobals;
using namespace ScalarGlobals;

#include "MpfrCoords.h"

#define FORMULA_MPFR
#include "../formulas/FractalFormulas.h"

#include "../scalar/ScalarRenderer.h"
using namespace ScalarRenderer;

#include "../util/InlineUtil.h"

static FORCE_INLINE void complexInverse_mp(mpfr_t &cr, mpfr_t &ci) {
    mpfr_t cmag;
    create_copy(cmag, add(mul(&cr, &cr), mul(&ci, &ci)));

    if (cmag != 0) {
        mpfr_set(cr, *div(&cr, &cmag), ROUNDING);
        mpfr_set(ci, *div(neg(&ci), &cmag), ROUNDING);
    }

    mpfr_clear(cmag);
}

namespace MpfrRenderer {
    FORCE_INLINE void initCoords_mp(mpfr_t &cr, mpfr_t &ci,
        mpfr_t &zr, mpfr_t &zi) {
        if (isInverse) {
            complexInverse_mp(cr, ci);
        }

        if (isJuliaSet) {
            mpfr_set(zr, cr, ROUNDING);
            mpfr_set(zi, ci, ROUNDING);

            mpfr_set(cr, seed_r_mp, ROUNDING);
            mpfr_set(ci, seed_i_mp, ROUNDING);
        } else {
            mpfr_set(zr, seed_r_mp, ROUNDING);
            mpfr_set(zi, seed_i_mp, ROUNDING);
        }
    }

    FORCE_INLINE int iterateFractalMpfr(const mpfr_t &cr, const mpfr_t &ci,
        mpfr_t &zr, mpfr_t &zi,
        mpfr_t &dr, mpfr_t &di,
        mpfr_t &mag) {
        mpfr_set_d(mag, 0, ROUNDING);
        int i = 0;

        mpfr_t zr2, zi2;
        mpfr_init2(zr2, PRECISION);
        mpfr_init2(zi2, PRECISION);

        for (; i < count; i++) {
            mpfr_mul(zr2, zr, zr, ROUNDING);
            mpfr_mul(zi2, zi, zi, ROUNDING);
            mpfr_add(mag, zr2, zi2, ROUNDING);

            if (mpfr_cmp(mag, bailout_mp) > 0) break;

            switch (colorMethod) {
                case 2:
                    derivative(zr, zi, dr, di, mag, dr, di);
                    break;
            }

            formula(cr, ci, zr, zi, zr2, zi2, mag, zr, zi);
        }

        mpfr_clears(zr2, zi2, nullptr);
        return i;
    }

    FORCE_INLINE void colorPixelMpfr(uint8_t *pixels, int &pos,
        int i, const mpfr_t &mag,
        const mpfr_t &zr, const mpfr_t &zi,
        const mpfr_t &dr, const mpfr_t &di) {
        const float mag_sc = mpfr_get_flt(mag, ROUNDING);

        const float zr_sc = mpfr_get_flt(zr, ROUNDING);
        const float zi_sc = mpfr_get_flt(zi, ROUNDING);

        const float dr_sc = mpfr_get_flt(dr, ROUNDING);
        const float di_sc = mpfr_get_flt(di, ROUNDING);

        colorPixelScalar(pixels, pos, i, mag_sc, zr_sc, zi_sc, dr_sc, di_sc);
    }

    void renderPixelMpfr(uint8_t *pixels, int &pos,
        int x, mpfr_t *ci) {
        mpfr_t cr;
        mpfr_init2(cr, PRECISION);

        getCenterReal_mp(cr, x);

        mpfr_t zr, zi;
        mpfr_init2(zr, PRECISION);
        mpfr_init2(zi, PRECISION);

        mpfr_t dr, di;
        init_set(dr, 1);
        init_set(di, 0);

        initCoords_mp(cr, *ci, zr, zi);

        mpfr_t mag;
        mpfr_init2(mag, PRECISION);

        int i = iterateFractalMpfr(cr, *ci, zr, zi, dr, di, mag);

        colorPixelMpfr(pixels, pos, i, mag, zr, zi, dr, di);

        mpfr_clears(cr, mag, zr, zi, dr, di, nullptr);
    }
}

#endif