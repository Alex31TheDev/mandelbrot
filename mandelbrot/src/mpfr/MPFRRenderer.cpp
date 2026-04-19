#ifdef USE_MPFR
#include "CommonDefs.h"
#include "MPFRRenderer.h"

#include <cstdint>

#include "MPFRGlobals.h"
#include "MPFRScratch.h"
#include "MPFRTypes.h"

#include "../scalar/ScalarGlobals.h"
#include "../scalar/ScalarRenderer.h"
using namespace MPFRGlobals;
using namespace ScalarGlobals;

#define FORMULA_MPFR
#define _SKIP_FORMULA_OPS

#include "../formula/fractals/mandelbrot.h"
#include "../formula/UndefFormulas.h"

#include "../formula/fractals/perpendicular.h"
#include "../formula/UndefFormulas.h"

#include "../formula/fractals/burningship.h"
#include "../formula/UndefFormulas.h"

#undef _SKIP_FORMULA_OPS
#undef FORMULA_MPFR

#include "MPFRCoords.h"

#include "util/InlineUtil.h"

static FORCE_INLINE MPFRScratch &getScratch() {
    thread_local MPFRScratch scratch;
    scratch.ensureReady();
    return scratch;
}

static FORCE_INLINE void complexInverse_mp(mpfr_t cr, mpfr_t ci, MPFRScratch &s) {
    mpfr_sqr(s.t[0], cr, MPFRTypes::ROUNDING);
    mpfr_sqr(s.t[1], ci, MPFRTypes::ROUNDING);
    mpfr_add(s.t[2], s.t[0], s.t[1], MPFRTypes::ROUNDING);

    if (mpfr_zero_p(s.t[2]) == 0) {
        mpfr_div(cr, cr, s.t[2], MPFRTypes::ROUNDING);
        mpfr_neg(s.t[3], ci, MPFRTypes::ROUNDING);
        mpfr_div(ci, s.t[3], s.t[2], MPFRTypes::ROUNDING);
    }
}

#define MP_DERIVATIVE_FOR(_FRACTAL_NAME) \
    if (normalPower) { \
        normalDerivative_##_FRACTAL_NAME##_mp(s); \
    } else if (wholePower) { \
        wholeDerivative_##_FRACTAL_NAME##_mp(s); \
    } else { \
        fractionalDerivative_##_FRACTAL_NAME##_mp(s); \
    }

#define MP_FORMULA_FOR(_FRACTAL_NAME) \
    if (normalPower) { \
        normalFormula_##_FRACTAL_NAME##_mp(cr, ci, s); \
    } else if (wholePower) { \
        wholeFormula_##_FRACTAL_NAME##_mp(cr, ci, s); \
    } else { \
        fractionalFormula_##_FRACTAL_NAME##_mp(cr, ci, s); \
    }

#define MP_ITERATE_FOR(_FRACTAL_NAME) \
    for (; i < count; ++i) { \
        mpfr_sqr(s.zr2, s.zr, MPFRTypes::ROUNDING); \
        mpfr_sqr(s.zi2, s.zi, MPFRTypes::ROUNDING); \
        mpfr_add(s.mag, s.zr2, s.zi2, MPFRTypes::ROUNDING); \
        if (mpfr_cmp(s.mag, bailout_mp) > 0) break; \
        if (colorMethod == 3) { \
            MP_DERIVATIVE_FOR(_FRACTAL_NAME); \
        } \
        MP_FORMULA_FOR(_FRACTAL_NAME); \
    }

namespace MPFRRenderer {
    void initCoordsMPFR(
        mpfr_t cr, mpfr_t ci,
        mpfr_t zr, mpfr_t zi,
        mpfr_t dr, mpfr_t di
    ) {
        MPFRScratch &s = getScratch();

        if (isInverse) {
            complexInverse_mp(cr, ci, s);
        }

        if (isJuliaSet) {
            mpfr_set(zr, cr, MPFRTypes::ROUNDING);
            mpfr_set(zi, ci, MPFRTypes::ROUNDING);

            mpfr_set(cr, seed_r_mp, MPFRTypes::ROUNDING);
            mpfr_set(ci, seed_i_mp, MPFRTypes::ROUNDING);
        } else {
            mpfr_set(zr, seed_r_mp, MPFRTypes::ROUNDING);
            mpfr_set(zi, seed_i_mp, MPFRTypes::ROUNDING);
        }

        mpfr_set_ui(dr, 1, MPFRTypes::ROUNDING);
        mpfr_set_ui(di, 0, MPFRTypes::ROUNDING);
    }

    int iterateFractalMPFR(
        mpfr_srcptr cr, mpfr_srcptr ci,
        mpfr_t zr, mpfr_t zi,
        mpfr_t dr, mpfr_t di,
        mpfr_t mag
    ) {
        MPFRScratch &s = getScratch();
        initFormulaConstants_mp(s);

        mpfr_set(s.zr, zr, MPFRTypes::ROUNDING);
        mpfr_set(s.zi, zi, MPFRTypes::ROUNDING);
        mpfr_set(s.dr, dr, MPFRTypes::ROUNDING);
        mpfr_set(s.di, di, MPFRTypes::ROUNDING);

        int i = 0;
        if (invalidPower) {
            mpfr_set(s.zr, cr, MPFRTypes::ROUNDING);
            mpfr_set(s.zi, ci, MPFRTypes::ROUNDING);
            mpfr_set_ui(s.dr, 0, MPFRTypes::ROUNDING);
            mpfr_set_ui(s.di, 0, MPFRTypes::ROUNDING);

            mpfr_sqr(s.zr2, s.zr, MPFRTypes::ROUNDING);
            mpfr_sqr(s.zi2, s.zi, MPFRTypes::ROUNDING);
            mpfr_add(s.mag, s.zr2, s.zi2, MPFRTypes::ROUNDING);

            i = mpfr_cmp(s.mag, bailout_mp) > 0 ? 0 : count;
        } else {
            switch (fractalType) {
                case 0:
                    MP_ITERATE_FOR(mandelbrot);
                    break;

                case 1:
                    MP_ITERATE_FOR(perpendicular);
                    break;

                case 2:
                    MP_ITERATE_FOR(burningship);
                    break;
            }
        }

        mpfr_set(zr, s.zr, MPFRTypes::ROUNDING);
        mpfr_set(zi, s.zi, MPFRTypes::ROUNDING);
        mpfr_set(dr, s.dr, MPFRTypes::ROUNDING);
        mpfr_set(di, s.di, MPFRTypes::ROUNDING);
        mpfr_set(mag, s.mag, MPFRTypes::ROUNDING);

        return i;
    }

    void colorPixelMPFR(
        uint8_t *pixels, size_t &pos,
        int i, mpfr_srcptr mag,
        mpfr_srcptr zr, mpfr_srcptr zi,
        mpfr_srcptr dr, mpfr_srcptr di
    ) {
        const scalar_full_t mag_sc =
            static_cast<scalar_full_t>(mpfr_get_d(mag, MPFRTypes::ROUNDING));
        const scalar_full_t zr_sc =
            static_cast<scalar_full_t>(mpfr_get_d(zr, MPFRTypes::ROUNDING));
        const scalar_full_t zi_sc =
            static_cast<scalar_full_t>(mpfr_get_d(zi, MPFRTypes::ROUNDING));
        const scalar_full_t dr_sc =
            static_cast<scalar_full_t>(mpfr_get_d(dr, MPFRTypes::ROUNDING));
        const scalar_full_t di_sc =
            static_cast<scalar_full_t>(mpfr_get_d(di, MPFRTypes::ROUNDING));

        ScalarRenderer::colorPixelScalar(
            pixels, pos,
            i, mag_sc,
            zr_sc, zi_sc,
            dr_sc, di_sc
        );
    }

    void renderPixelMPFR(
        uint8_t *pixels, size_t &pos,
        int x, mpfr_srcptr ci, uint64_t *totalIterCount
    ) {
        MPFRScratch &s = getScratch();

        getCenterReal_mp(s.cr, x);
        mpfr_set(s.ci, ci, MPFRTypes::ROUNDING);

        if (isInverse) complexInverse_mp(s.cr, s.ci, s);

        if (isJuliaSet) {
            mpfr_set(s.zr, s.cr, MPFRTypes::ROUNDING);
            mpfr_set(s.zi, s.ci, MPFRTypes::ROUNDING);
            mpfr_set(s.cr, seed_r_mp, MPFRTypes::ROUNDING);
            mpfr_set(s.ci, seed_i_mp, MPFRTypes::ROUNDING);
        } else {
            mpfr_set(s.zr, seed_r_mp, MPFRTypes::ROUNDING);
            mpfr_set(s.zi, seed_i_mp, MPFRTypes::ROUNDING);
        }

        mpfr_set_ui(s.dr, 1, MPFRTypes::ROUNDING);
        mpfr_set_ui(s.di, 0, MPFRTypes::ROUNDING);

        const int i = iterateFractalMPFR(
            s.cr, s.ci,
            s.zr, s.zi,
            s.dr, s.di,
            s.mag
        );

        colorPixelMPFR(
            pixels, pos,
            i, s.mag,
            s.zr, s.zi,
            s.dr, s.di
        );

        if (totalIterCount) {
            *totalIterCount = static_cast<uint64_t>(i) + 1;
        }
    }
}

#endif