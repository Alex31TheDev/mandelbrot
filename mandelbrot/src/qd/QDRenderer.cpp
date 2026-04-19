#ifdef USE_QD
#include "CommonDefs.h"
#include "QDRenderer.h"

#include <cstdint>

#include "../scalar/ScalarTypes.h"
#include "QDTypes.h"

#include "QDGlobals.h"
#include "../scalar/ScalarGlobals.h"
using namespace QDGlobals;
using namespace ScalarGlobals;

#include "QDCoords.h"
#include "../scalar/ScalarRenderer.h"

#define FORMULA_QD
#define _SKIP_FORMULA_OPS
#include "../formula/FormulaTypes.h"

#include "util/InlineUtil.h"

FORCE_INLINE void complexInverse_qd(
    qd_number_t &cr, qd_number_t &ci
) {
    const qd_number_t cmag = cr * cr + ci * ci;

    if (cmag != 0.0) {
        cr = cr / cmag;
        ci = -ci / cmag;
    }
}

FORCE_INLINE void _initCoords_qd(
    qd_number_t &cr, qd_number_t &ci,
    qd_number_t &zr, qd_number_t &zi,
    qd_number_t &dr, qd_number_t &di
) {
    if (isInverse) complexInverse_qd(cr, ci);

    if (isJuliaSet) {
        zr = cr;
        zi = ci;

        cr = seed_r_qd;
        ci = seed_i_qd;
    } else {
        zr = seed_r_qd;
        zi = seed_i_qd;
    }

    dr = 1.0;
    di = 0.0;
}

FORCE_INLINE int _iterateFractal_qd(
    qd_param_t cr, qd_param_t ci,
    qd_number_t &zr, qd_number_t &zi,
    qd_number_t &dr, qd_number_t &di,
    qd_number_t &mag
) {
    mag = 0.0;
    int i = 0;

    if (invalidPower) {
        zr = cr;
        zi = ci;
        dr = di = 0.0;
        mag = zr * zr + zi * zi;

        i = mag > bailout_qd ? 0 : count;
    } else {
        switch (fractalType) {
            case 0:
#define _FRACTAL_TYPE mandelbrot
#include "loop/FractalLoop.h"
#undef _FRACTAL_TYPE
                break;

            case 1:
#define _FRACTAL_TYPE perpendicular
#include "loop/FractalLoop.h"
#undef _FRACTAL_TYPE
                break;

            case 2:
#define _FRACTAL_TYPE burningship
#include "loop/FractalLoop.h"
#undef _FRACTAL_TYPE
                break;
        }
    }

    return i;
}

FORCE_INLINE void _colorPixel_qd(
    uint8_t *pixels, size_t &pos,
    int i, qd_param_t mag,
    qd_param_t zr, qd_param_t zi,
    qd_param_t dr, qd_param_t di
) {
    const scalar_half_t mag_sc = static_cast<float>(to_double(mag));

    const scalar_half_t zr_sc = static_cast<float>(to_double(zr));
    const scalar_half_t zi_sc = static_cast<float>(to_double(zi));

    const scalar_half_t dr_sc = static_cast<float>(to_double(dr));
    const scalar_half_t di_sc = static_cast<float>(to_double(di));

    ScalarRenderer::colorPixelScalar(
        pixels, pos,
        i, mag_sc,
        zr_sc, zi_sc,
        dr_sc, di_sc
    );
}

namespace QDRenderer {
    void initCoordsQD(
        qd_number_t &cr, qd_number_t &ci,
        qd_number_t &zr, qd_number_t &zi,
        qd_number_t &dr, qd_number_t &di
    ) {
        _initCoords_qd(
            cr, ci,
            zr, zi,
            dr, di
        );
    }

    int iterateFractalQD(
        qd_param_t cr, qd_param_t ci,
        qd_number_t &zr, qd_number_t &zi,
        qd_number_t &dr, qd_number_t &di,
        qd_number_t &mag
    ) {
        return _iterateFractal_qd(
            cr, ci,
            zr, zi,
            dr, di,
            mag
        );
    }

    void colorPixelQD(
        uint8_t *pixels, size_t &pos,
        int i, qd_param_t mag,
        qd_param_t zr, qd_param_t zi,
        qd_param_t dr, qd_param_t di
    ) {
        _colorPixel_qd(
            pixels, pos,
            i, mag,
            zr, zi,
            dr, di
        );
    }

    void renderPixelQD(
        uint8_t *pixels, size_t &pos,
        int x, qd_number_t ci, uint64_t *totalIterCount
    ) {
        qd_number_t cr = getCenterReal_qd(x);

        qd_number_t zr, zi;
        qd_number_t dr, di;
        _initCoords_qd(
            cr, ci,
            zr, zi,
            dr, di
        );

        qd_number_t mag;
        const int i = _iterateFractal_qd(
            cr, ci,
            zr, zi,
            dr, di,
            mag
        );

        _colorPixel_qd(
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
