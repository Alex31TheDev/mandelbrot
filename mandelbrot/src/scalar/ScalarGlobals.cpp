#include "CommonDefs.h"
#include "ScalarGlobals.h"

#include "ScalarTypes.h"

#include "../render/RenderGlobals.h"
using namespace RenderGlobals;

namespace ScalarGlobals {
    int count, colorMethod, fractalType;

    bool isJuliaSet, isInverse;
    bool normalSeed;
    bool invalidPower, normalPower, wholePower, fractionalPower;
    bool useQuadPath;

    scalar_full_t halfWidth, halfHeight, invWidth, invHeight;
    scalar_full_t realScale, imagScale;

    scalar_full_t point_r, point_i, seed_r, seed_i;
    scalar_full_t N;

    scalar_half_t zoom, aspect;
    scalar_half_t invCount, invLnPow;

    scalar_half_t freq_r, freq_g, freq_b, freqMult;
    scalar_half_t phase_r, phase_g, phase_b, cosPhase;
    scalar_half_t light_r, light_i, light_h;

    ScalarColorPalette palette({}, SC_SYM_H(10.0));

    void initImageValues() {
        aspect = CAST_H(width) / height;

        halfWidth = CAST_F(width) / SC_SYM_F(2.0);
        halfHeight = CAST_F(height) / SC_SYM_F(2.0);

        invWidth = RECIP_F(width);
        invHeight = RECIP_F(height);

        const scalar_full_t zoomPow = POW_F(10, zoom);

        realScale = RECIP_F(zoomPow);
        imagScale = realScale / aspect;
    }

    bool setZoomGlobals(int iterCount, scalar_half_t zoomScale) {
        if (iterCount < 1) count = MIN_ITERATIONS;
        else count = iterCount;

        zoom = zoomScale;
        initImageValues();

        if (iterCount == 0) {
            const scalar_half_t visualRange = CAST_H(POW_F(10, zoom));
            count += static_cast<int>(POW_H(LOG10_H(visualRange), 5));
        }

        invCount = RECIP_H(count);
        return zoomScale > SC_SYM_H(-3.25);
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

    bool setFractalType(int type, bool julia, bool inverse) {
        fractalType = type;
        isJuliaSet = julia;
        isInverse = inverse;
        useQuadPath = fractalType == 0 && normalPower;
        return true;
    }

    bool setColorMethod(int method) {
        colorMethod = method;
        return true;
    }

    bool setFractalExponent(scalar_full_t pw) {
        N = pw;
        invLnPow = RECIP_H(LOG_H(N));

        invalidPower = N <= ONE_F;
        normalPower = N == SC_SYM_F(2.0);
        wholePower = ISWHOLE_F(N);
        fractionalPower = !wholePower;
        useQuadPath = fractalType == 0 && normalPower;

        return !invalidPower;
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

    bool setPaletteGlobals(
        const std::vector<ScalarPaletteColor> &entries,
        scalar_half_t totalLength, scalar_half_t offset
    ) {
        if (entries.size() < 2 || ISNEG0_H(totalLength) || offset < ZERO_H) {
            return false;
        }

        palette = ScalarColorPalette(entries, totalLength, offset);
        return true;
    }
}