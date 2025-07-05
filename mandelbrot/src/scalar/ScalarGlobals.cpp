#include "CommonDefs.h"
#include "ScalarGlobals.h"

#include "ScalarTypes.h"

#include "../render/RenderGlobals.h"
using namespace RenderGlobals;

namespace ScalarGlobals {
    int count, colorMethod;

    bool isJuliaSet, isInverse;
    bool normalSeed;
    bool invalidPower, circlePower, normalPower, wholePower;

    scalar_full_t halfWidth, halfHeight, invWidth, invHeight;
    scalar_full_t realScale, imagScale;

    scalar_full_t point_r, point_i, seed_r, seed_i;
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
        if (iterCount < 1) count = MIN_ITERATIONS;
        else count = iterCount;

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

    bool setZoomPoints(
        scalar_full_t pr, scalar_full_t pi,
        scalar_full_t sr, scalar_full_t si
    ) {
        point_r = pr;
        point_i = pi;

        seed_r = sr;
        seed_i = si;

        normalSeed = IS0_F(seed_r) && IS0_F(seed_i);

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

    bool setFractalType(bool julia, bool inverse) {
        isJuliaSet = julia;
        isInverse = inverse;

        return true;
    }

    bool setColorGlobals(
        scalar_half_t freqR, scalar_half_t freqG, scalar_half_t freqB,
        scalar_half_t totalMult,
        scalar_half_t phaseR, scalar_half_t phaseG, scalar_half_t phaseB,
        scalar_half_t totalPhase
    ) {
        if (ABS_H(totalMult) <= SC_SYM_H(0.0001)) {
            return false;
        }

        freqMult = totalMult;
        freq_r = freqR * freqMult;
        freq_g = freqG * freqMult;
        freq_b = freqB * freqMult;

        cosPhase = totalPhase;
        phase_r = phaseR + cosPhase;
        phase_g = phaseG + cosPhase;
        phase_b = phaseB + cosPhase;

        return true;
    }

    bool setLightGlobals(scalar_half_t lr, scalar_half_t li) {
        const scalar_half_t mag = SQRT_H(lr * lr + li * li);
        if (ISNEG0_H(mag)) return false;

        light_r = li / mag;
        light_i = li / mag;
        light_h = mag;

        return true;
    }
}