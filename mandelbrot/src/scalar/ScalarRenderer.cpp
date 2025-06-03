#include "ScalarRenderer.h"

#include <cstdint>
#include <cmath>

#include "ScalarTypes.h"

#include "ScalarGlobals.h"
using namespace ScalarGlobals;

#include "ScalarCoords.h"

#define FORMULA_SCALAR
#include "../formulas/FractalFormulas.h"

static inline void complexInverse(scalar_full_t &cr, scalar_full_t &ci) {
    const scalar_full_t cmag = cr * cr + ci * ci;

    if (cmag != 0) {
        cr = cr / cmag;
        ci = -ci / cmag;
    }
}

static void initCoords(scalar_full_t &cr, scalar_full_t &ci,
    scalar_full_t &zr, scalar_full_t &zi) {
    if (isInverse) {
        complexInverse(cr, ci);
    }

    if (isJuliaSet) {
        zr = cr;
        zi = ci;

        cr = seed_r;
        ci = seed_i;
    } else {
        zr = seed_r;
        zi = seed_i;
    }
}

static const scalar_half_t getIterVal(const int i) {
#ifdef NORM_ITER_COUNT
    return i * invCount;
#else
    return CAST_H(i);
#endif
}

static const scalar_half_t getSmoothIterVal(const int i, const scalar_half_t mag) {
    const scalar_half_t sqrt_mag = SQRT_H(mag);
    const scalar_half_t m = LOG_H(LOG_H(sqrt_mag) * invLnBail) * invLnPow;
    return i - m;
}

static const scalar_half_t normCos(const scalar_half_t x) {
    return (COS_H(x) + 1) * SC_SYM_H(0.5);
}

static void getColorPixel(const scalar_half_t val,
    scalar_half_t &outR, scalar_half_t &outG, scalar_half_t &outB) {
    const scalar_half_t R_x = val * freq_r + phase_r;
    const scalar_half_t G_x = val * freq_g + phase_g;
    const scalar_half_t B_x = val * freq_b + phase_b;

    outR = normCos(R_x);
    outG = normCos(G_x);
    outB = normCos(B_x);
}

static const scalar_half_t getLightVal(const scalar_full_t zr, const scalar_full_t zi,
    const scalar_full_t dr, const scalar_full_t di) {
    const scalar_half_t dsum = RECIP_H(dr * dr + di * di);
    scalar_half_t ur = CAST_H(zr * dr + zi * di) * dsum;
    scalar_half_t ui = CAST_H(zi * dr - zr * di) * dsum;

    const scalar_half_t umag = RECIP_H(SQRT_H(ur * ur + ui * ui));
    ur *= umag;
    ui *= umag;

    scalar_half_t light = ur * light_r - ui * light_i + light_h;
    light /= (light_h + 1);

#ifdef NORM_ITER_COUNT
    return MAX_H(light, 0) * invCount;
#else
    return MAX_H(light, 0);
#endif
}

namespace ScalarRenderer {
    const int iterateFractalScalar(const scalar_full_t cr, const scalar_full_t ci,
        scalar_full_t &zr, scalar_full_t &zi,
        scalar_full_t &dr, scalar_full_t &di,
        scalar_full_t &mag) {
        mag = 0;
        int i = 0;

        for (; i < count; i++) {
            const scalar_full_t zr2 = zr * zr;
            const scalar_full_t zi2 = zi * zi;
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

    const uint8_t pixelToInt(const scalar_half_t val) {
        scalar_half_t conv_val = val * 255;
        conv_val = MIN_H(MAX_H(val, 0), 255);
        return CAST_INT_U(conv_val, 8);
    }

    inline void setPixel(uint8_t *pixels, int &pos,
        const scalar_half_t R, const scalar_half_t G, const scalar_half_t B) {
        pixels[pos++] = pixelToInt(R);
        pixels[pos++] = pixelToInt(G);
        pixels[pos++] = pixelToInt(B);
    }

    void colorPixelScalar(uint8_t *pixels, int &pos,
        const int i, const scalar_full_t mag,
        const scalar_full_t zr, const scalar_full_t zi,
        const scalar_full_t dr, const scalar_full_t di) {
        if (i == count) {
            ScalarRenderer::setPixel(pixels, pos, 0, 0, 0);
            return;
        }

        scalar_half_t val, R, G, B;

        switch (colorMethod) {
            case 0:
                val = getIterVal(i);
                getColorPixel(val, R, G, B);
                break;

            case 1:
                val = getSmoothIterVal(i, CAST_H(mag));
                getColorPixel(val, R, G, B);
                break;

            case 2:
                val = getLightVal(zr, zi, dr, di);
                R = G = B = val;
                break;
        }

        ScalarRenderer::setPixel(pixels, pos, R, G, B);
    }

    void renderPixelScalar(uint8_t *pixels, int &pos,
        const int x, scalar_full_t ci) {
        scalar_full_t cr = getCenterReal(x);

        scalar_full_t zr, zi;
        scalar_full_t dr = 1, di = 0;

        initCoords(cr, ci, zr, zi);

        scalar_full_t mag = 0;
        int i = iterateFractalScalar(cr, ci, zr, zi, dr, di, mag);

        colorPixelScalar(pixels, pos, i, mag, zr, zi, dr, di);
    }
}