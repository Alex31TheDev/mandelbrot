#ifdef USE_MPFR
#include "CommonDefs.h"
#include "MPFRRenderer.h"

#include <cstdint>

#include "../scalar/ScalarTypes.h"
#include "MpfrTypes.h"

#include "MPFRGlobals.h"
#include "../scalar/ScalarGlobals.h"
using namespace MPFRGlobals;
using namespace ScalarGlobals;

#include "MPFRCoords.h"
#include "../scalar/ScalarRenderer.h"

#define FORMULA_MPFR
#include "../formula/FractalFormulas.h"

#include "../util/InlineUtil.h"

FORCE_INLINE void complexInverse_mp(
    mpfr_number_t &cr, mpfr_number_t &ci
) {
    const mpfr_number_t cmag = cr * cr + ci * ci;

    if (cmag != 0.0) {
        cr = cr / cmag;
        ci = -ci / cmag;
    }
}

FORCE_INLINE void _initCoords_mp(
    mpfr_number_t &cr, mpfr_number_t &ci,
    mpfr_number_t &zr, mpfr_number_t &zi,
    mpfr_number_t &dr, mpfr_number_t &di
) {
    if (isInverse) complexInverse_mp(cr, ci);

    if (isJuliaSet) {
        zr = cr;
        zi = ci;

        cr = seed_r_mp;
        ci = seed_i_mp;
    } else {
        zr = seed_r_mp;
        zi = seed_i_mp;
    }

    dr = 1.0;
    di = 0.0;
}

FORCE_INLINE int _iterateFractal_mp(
    mpfr_param_t cr, mpfr_param_t ci,
    mpfr_number_t &zr, mpfr_number_t &zi,
    mpfr_number_t &dr, mpfr_number_t &di,
    mpfr_number_t &mag
) {
    mag = 0.0;
    int i = 0;

    for (; i < count; i++) {
        const mpfr_number_t zr2 = zr * zr;
        const mpfr_number_t zi2 = zi * zi;
        mag = zr2 + zi2;

        if (mag > bailout_mp) break;

        switch (colorMethod) {
            case 2:
                derivative(
                    zr, zi,
                    dr, di,
                    mag,
                    dr, di
                );
                break;
        }

        formula(
            cr, ci,
            zr, zi,
            mag,
            zr, zi
        );
    }

    return i;
}

FORCE_INLINE void _colorPixel_mp(
    uint8_t *pixels, size_t &pos,
    int i, mpfr_param_t mag,
    mpfr_param_t zr, mpfr_param_t zi,
    mpfr_param_t dr, mpfr_param_t di
) {
    const scalar_half_t mag_sc = CAST_H(mag);

    const scalar_half_t zr_sc = CAST_H(zr);
    const scalar_half_t zi_sc = CAST_H(zi);

    const scalar_half_t dr_sc = CAST_H(dr);
    const scalar_half_t di_sc = CAST_H(di);

    ScalarRenderer::colorPixelScalar(
        pixels, pos,
        i, mag_sc,
        zr_sc, zi_sc,
        dr_sc, di_sc
    );
}

namespace MPFRRenderer {
    void initCoordsMPFR(
        mpfr_number_t &cr, mpfr_number_t &ci,
        mpfr_number_t &zr, mpfr_number_t &zi,
        mpfr_number_t &dr, mpfr_number_t &di
    ) {
        _initCoords_mp(
            cr, ci,
            zr, zi,
            dr, di
        );
    }

    int iterateFractalMPFR(
        mpfr_param_t cr, mpfr_param_t ci,
        mpfr_number_t &zr, mpfr_number_t &zi,
        mpfr_number_t &dr, mpfr_number_t &di,
        mpfr_number_t &mag
    ) {
        return _iterateFractal_mp(
            cr, ci,
            zr, zi,
            dr, di,
            mag
        );
    }

    void colorPixelMPFR(
        uint8_t *pixels, size_t &pos,
        int i, mpfr_param_t mag,
        mpfr_param_t zr, mpfr_param_t zi,
        mpfr_param_t dr, mpfr_param_t di
    ) {
        _colorPixel_mp(
            pixels, pos,
            i, mag,
            zr, zi,
            dr, di
        );
    }

    void renderPixelMPFR(
        uint8_t *pixels, size_t &pos,
        int x, mpfr_number_t ci, uint64_t *totalIterCount
    ) {
        mpfr_number_t cr = getCenterReal_mp(x);

        mpfr_number_t zr, zi;
        mpfr_number_t dr, di;
        _initCoords_mp(
            cr, ci,
            zr, zi,
            dr, di
        );

        mpfr_number_t mag;
        const int i = _iterateFractal_mp(
            cr, ci,
            zr, zi,
            dr, di,
            mag
        );

        _colorPixel_mp(
            pixels, pos,
            i, mag,
            zr, zi,
            dr, di
        );

        if (totalIterCount) {
            *totalIterCount = static_cast<uint64_t>(i);
        }
    }
}

#endif