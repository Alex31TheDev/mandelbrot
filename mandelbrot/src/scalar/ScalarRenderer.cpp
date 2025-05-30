#include "ScalarTypes.h"
#include "ScalarRenderer.h"

#include <cstdint>
#include <cmath>

#include "ScalarCoords.h"

#include "ScalarGlobals.h"
using namespace ScalarGlobals;

static inline scalar_half_t normCos(scalar_half_t x) {
    return (COS_H(x) + 1) * SCALAR_SYM_H(0.5);
}

static inline void getColorPixel(scalar_half_t val,
    scalar_half_t &outR, scalar_half_t &outG, scalar_half_t &outB) {
    scalar_half_t R_x = phase_r + val * freq_r;
    scalar_half_t G_x = phase_g + val * freq_g;
    scalar_half_t B_x = phase_b + val * freq_b;

    outR = normCos(R_x);
    outG = normCos(G_x);
    outB = normCos(B_x);
}

static inline scalar_half_t getSmoothIterVal(int i, scalar_half_t mag) {
    scalar_half_t sqrt_mag = SQRT_H(mag);
    scalar_half_t m = LOG_H(LOG_H(sqrt_mag) * invLnBail) * invLn2;
    return CAST_H(i) - m;
}

static inline float getLightVal(scalar_full_t zr, scalar_full_t zi, scalar_full_t dr, scalar_full_t di) {
    scalar_half_t dsum = RECIP_H(dr * dr + di * di);
    scalar_half_t ur = CAST_H(zr * dr + zi * di) * dsum;
    scalar_half_t ui = CAST_H(zi * dr - zr * di) * dsum;

    scalar_half_t umag = RECIP_H(SQRT_H(ur * ur + ui * ui));
    ur *= umag;
    ui *= umag;

    scalar_half_t light = (ur * light_r + ui * light_i + light_h) / (light_h + 1);
    return MIN_H(light, 0);
}

namespace ScalarRenderer {
    inline uint8_t pixelToInt(scalar_half_t val) {
        val *= 255;
        val = MIN_H(MAX_H(val, 0), 255);
        return CAST_INT_U(val, 8);
    }

    inline void setPixel(uint8_t *pixels, int &pos,
        scalar_half_t R, scalar_half_t G, scalar_half_t B) {
        pixels[pos++] = pixelToInt(R);
        pixels[pos++] = pixelToInt(G);
        pixels[pos++] = pixelToInt(B);
    }

    void renderPixelScalar(uint8_t *pixels, int &pos,
        int x, scalar_full_t ci) {
        scalar_full_t cr = getCenterReal(x);

        if (isInverse) {
            scalar_full_t cmag = cr * cr + ci * ci;

            if (cmag != 0) {
                cr = cr / cmag;
                ci = -ci / cmag;
            }
        }

        scalar_full_t zr, zi;
        scalar_full_t dr = 1, di = 0;

        if (isJuliaSet) {
            zr = cr;
            zi = ci;

            cr = seed_r;
            ci = seed_i;
        } else {
            zr = seed_r;
            zi = seed_i;
        }

        int i = 0;
        scalar_full_t mag = 0;

        for (; i < count; i++) {
            scalar_full_t zr2 = zr * zr;
            scalar_full_t zi2 = zi * zi;
            mag = zr2 + zi2;

            if (mag > BAILOUT) break;

            switch (colorMethod) {
                case 1:
                {
                    scalar_full_t dr2 = dr;
                    dr = 2 * (zr * dr - zi * di) + 1;
                    di = 2 * (zr * di + zi * dr2);
                }
                break;
            }

            zi = 2 * zr * zi + ci;
            zr = zr2 - zi2 + cr;
        }

        if (i == count) {
            setPixel(pixels, pos, 0, 0, 0);
            return;
        }

        switch (colorMethod) {
            case 0:
            {
                scalar_half_t val = getSmoothIterVal(i, CAST_H(mag));

                scalar_half_t R, G, B;
                getColorPixel(val, R, G, B);

                setPixel(pixels, pos, R, G, B);
            }
            break;

            case 1:
            {
                scalar_half_t val = getLightVal(zr, zi, dr, di);
                setPixel(pixels, pos, val, val, val);
            }
            break;
        }
    }
}