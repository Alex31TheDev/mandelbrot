#include "CommonDefs.h"
#include "ScalarRenderer.h"

#include <cstdint>

#include "ScalarTypes.h"

#include "ScalarGlobals.h"
using namespace ScalarGlobals;

#include "../image/Image.h"
#include "../util/InlineUtil.h"

#ifdef USE_SCALAR
#include "ScalarCoords.h"

#define FORMULA_SCALAR
#include "../formula/FractalFormulas.h"

FORCE_INLINE void complexInverse(
    scalar_full_t &cr, scalar_full_t &ci
) {
    const scalar_full_t cmag = cr * cr + ci * ci;

    if (ISNOT0_F(cmag)) {
        cr = cr / cmag;
        ci = -ci / cmag;
    }
}

FORCE_INLINE void _initCoords(
    scalar_full_t &cr, scalar_full_t &ci,
    scalar_full_t &zr, scalar_full_t &zi,
    scalar_full_t &dr, scalar_full_t &di
) {
    if (isInverse) complexInverse(cr, ci);

    if (isJuliaSet) {
        zr = cr;
        zi = ci;

        cr = seed_r;
        ci = seed_i;
    } else {
        zr = seed_r;
        zi = seed_i;
    }

    dr = ONE_F;
    di = ZERO_F;
}

FORCE_INLINE int _iterateFractal(
    scalar_full_t cr, scalar_full_t ci,
    scalar_full_t &zr, scalar_full_t &zi,
    scalar_full_t &dr, scalar_full_t &di,
    scalar_full_t &mag
) {
    mag = ONE_F;
    int i = 0;

    for (; i < count; i++) {
        const scalar_full_t zr2 = zr * zr;
        const scalar_full_t zi2 = zi * zi;
        mag = zr2 + zi2;

        if (mag > BAILOUT) break;

        switch (colorMethod) {
            case 2:
                derivative(
                    zr, zi,
                    dr, di,
                    mag,
                    dr, di
                );
                break;

            default:
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

#endif

#if USE_SCALAR_COLORING

FORCE_INLINE scalar_half_t getIterVal(int i) {
#ifdef NORM_ITER_COUNT
    return i * invCount;
#else
    return CAST_H(i);
#endif
}

FORCE_INLINE scalar_half_t getSmoothIterVal(int i, scalar_half_t mag) {
    const scalar_half_t sqrt_mag = SQRT_H(mag);
    const scalar_half_t m = LOG_H(LOG_H(sqrt_mag) * invLnBail) * invLnPow;
    return i - m;
}

FORCE_INLINE scalar_half_t normCos(scalar_half_t x) {
    return (COS_H(x) + ONE_H) * ONEHALF_H;
}

FORCE_INLINE void getPixelColor(
    scalar_half_t val,
    scalar_half_t &outR, scalar_half_t &outG, scalar_half_t &outB
) {
#if false
    const scalar_half_t R_x = val * freq_r + phase_r;
    const scalar_half_t G_x = val * freq_g + phase_g;
    const scalar_half_t B_x = val * freq_b + phase_b;

    outR = normCos(R_x * 10);
    outG = normCos(G_x * 10);
    outB = normCos(B_x * 10);
#else
    palette.sample(val, outR, outG, outB);
#endif
}

FORCE_INLINE scalar_half_t getLightVal(
    scalar_full_t zr, scalar_full_t zi,
    scalar_full_t dr, scalar_full_t di
) {
    const scalar_half_t dsum = RECIP_H(dr * dr + di * di);
    scalar_half_t ur = CAST_H(zr * dr + zi * di) * dsum;
    scalar_half_t ui = CAST_H(zi * dr - zr * di) * dsum;

    const scalar_half_t umag = RECIP_H(SQRT_H(ur * ur + ui * ui));
    ur *= umag;
    ui *= umag;

    scalar_half_t light = ur * light_r - ui * light_i + light_h;
    light /= (light_h + ONE_H);

#ifdef NORM_ITER_COUNT
    return MAX_H(light, ZERO_H) * invCount;
#else
    return MAX_H(light, ZERO_H);
#endif
}

#endif

FORCE_INLINE uint8_t _colorToInt(scalar_half_t val) {
    scalar_half_t newVal = val * SC_SYM_H(255.0);
    newVal = CLAMP_H(newVal, 0, 255);
    return CAST_INT_U(newVal, 8);
}

FORCE_INLINE void _setPixel(
    uint8_t *pixels, size_t &pos,
    scalar_half_t R, scalar_half_t G, scalar_half_t B
) {
    if (R + G + B < COLOR_EPS) {
        pos += Image::STRIDE;
    } else {
        pixels[pos++] = _colorToInt(R);
        pixels[pos++] = _colorToInt(G);
        pixels[pos++] = _colorToInt(B);
    }
}

#if USE_SCALAR_COLORING
FORCE_INLINE void _colorPixel(
    uint8_t *pixels, size_t &pos,
    int i, scalar_full_t mag,
    scalar_full_t zr, scalar_full_t zi,
    scalar_full_t dr, scalar_full_t di
) {
    if (i == count) {
        _setPixel(pixels, pos, ZERO_H, ZERO_H, ZERO_H);
        return;
    }

    scalar_half_t val,
        R = ZERO_H, G = ZERO_H, B = ZERO_H;

    switch (colorMethod) {
        case 0:
            val = getIterVal(i);
            getPixelColor(val, R, G, B);
            break;

        case 1:
            val = getSmoothIterVal(i, CAST_H(mag));
            getPixelColor(val, R, G, B);
            break;

        case 2:
            val = getLightVal(zr, zi, dr, di);
            R = G = B = val;
            break;

        default:
            break;
    }

    _setPixel(pixels, pos, R, G, B);
}
#endif

namespace ScalarRenderer {
#ifdef USE_SCALAR
    void initCoordsScalar(
        scalar_full_t &cr, scalar_full_t &ci,
        scalar_full_t &zr, scalar_full_t &zi,
        scalar_full_t &dr, scalar_full_t &di
    ) {
        _initCoords(
            cr, ci,
            zr, zi,
            dr, di
        );
    }

    int iterateFractalScalar(
        scalar_full_t cr, scalar_full_t ci,
        scalar_full_t &zr, scalar_full_t &zi,
        scalar_full_t &dr, scalar_full_t &di,
        scalar_full_t &mag
    ) {
        return _iterateFractal(
            cr, ci,
            zr, zi,
            dr, di,
            mag
        );
    }
#endif

    uint8_t colorToInt(scalar_half_t val) {
        return _colorToInt(val);
    }

    void setPixel(
        uint8_t *pixels, size_t &pos,
        scalar_half_t R, scalar_half_t G, scalar_half_t B
    ) {
        _setPixel(
            pixels, pos,
            R, G, B
        );
    }

#if USE_SCALAR_COLORING
    void colorPixelScalar(
        uint8_t *pixels, size_t &pos,
        int i, scalar_full_t mag,
        scalar_full_t zr, scalar_full_t zi,
        scalar_full_t dr, scalar_full_t di
    ) {
        _colorPixel(
            pixels, pos,
            i, mag,
            zr, zi,
            dr, di
        );
    }
#endif

#ifdef USE_SCALAR
    void renderPixelScalar(
        uint8_t *pixels, size_t &pos,
        int x, scalar_full_t ci, uint64_t *totalIterCount
    ) {
        scalar_full_t cr = getCenterReal(x);

        scalar_full_t zr, zi;
        scalar_full_t dr, di;
        _initCoords(
            cr, ci,
            zr, zi,
            dr, di
        );

        scalar_full_t mag;
        const int i = _iterateFractal(
            cr, ci,
            zr, zi,
            dr, di,
            mag
        );

        _colorPixel(
            pixels, pos,
            i, mag,
            zr, zi,
            dr, di
        );

        if (totalIterCount) {
            *totalIterCount = static_cast<uint64_t>(i);
        }
    }
#endif
}