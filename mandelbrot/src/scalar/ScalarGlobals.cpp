#include "CommonDefs.h"
#include "ScalarGlobals.h"

#include "ScalarTypes.h"

#include "../render/RenderGlobals.h"
using namespace RenderGlobals;

namespace ScalarGlobals {
    int count, colorMethod;

    bool isJuliaSet = false, isInverse = false;
    bool invalidPower, circlePower, normalPower, wholePower;

    scalar_full_t halfWidth, halfHeight, invWidth, invHeight;
    scalar_full_t realScale, imagScale;

    scalar_full_t point_r = DEFAULT_POINT_R, point_i = DEFAULT_POINT_I;
    scalar_full_t seed_r = DEFAULT_SEED_R, seed_i = DEFAULT_SEED_I;
    scalar_full_t N;

    scalar_half_t zoom, aspect;
    scalar_half_t invCount, invLnPow;

    scalar_half_t freq_r, freq_g, freq_b, freqMult;
    scalar_half_t phase_r, phase_g, phase_b, cosPhase;
    scalar_half_t light_r, light_i, light_h;

    void initImageValues() {
        aspect = CAST_H(width) / height;

        halfWidth = CAST_F(width) / SC_SYM_F(2.0);
        halfHeight = CAST_F(height) / SC_SYM_F(2.0);

        invWidth = RECIP_F(width);
        invHeight = RECIP_F(height);
    }

    bool setZoomGlobals(int iterCount, scalar_half_t zoomScale) {
        if (iterCount < MIN_ITERATIONS) {
            count = MIN_ITERATIONS;
        } else {
            count = iterCount;
        }

        if (zoomScale < SC_SYM_H(-3.25)) return false;
        zoom = zoomScale;

        const scalar_full_t zoomPow = POW_F(10, zoom);

        realScale = RECIP_F(zoomPow);
        imagScale = realScale / aspect;

        if (iterCount == 0) {
            const scalar_half_t visualRange = CAST_H(zoomPow);
            count += static_cast<int>(POW_H(LOG10_H(visualRange), 5));
        }

        invCount = RECIP_H(count);
        return true;
    }

    bool setFractalExponent(scalar_full_t pw) {
        if (pw <= ONE_F) return false;

        N = pw;

        invalidPower = IS0_F(N);
        circlePower = N == SC_SYM_F(1.0);
        normalPower = N == SC_SYM_F(2.0);
        wholePower = ISWHOLE_F(N);

        invLnPow = RECIP_H(LOG_H(pw));
        return true;
    }

    bool setColorGlobals(scalar_half_t R, scalar_half_t G, scalar_half_t B,
        scalar_half_t mult) {
        cosPhase = DEFAULT_COS_PHASE;
        phase_r = cosPhase + DEFAULT_PHASE_R;
        phase_g = cosPhase + DEFAULT_PHASE_G;
        phase_b = cosPhase + DEFAULT_PHASE_R;

        if (ABS_H(mult) <= SC_SYM_H(0.0001)) return false;

        freq_r = R * mult;
        freq_g = G * mult;
        freq_b = B * mult;
        freqMult = mult;

        return true;
    }

    bool setLightGlobals(scalar_half_t real, scalar_half_t imag) {
        const scalar_half_t mag = SQRT_H(real * real + imag * imag);
        if (ISNEG0_H(mag)) return false;

        light_r = real / mag;
        light_i = imag / mag;
        light_h = mag;

        return true;
    }
}