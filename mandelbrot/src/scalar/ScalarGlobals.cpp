#include "CommonDefs.h"
#include "ScalarGlobals.h"

#include <algorithm>
#include <vector>

#include "ScalarTypes.h"

#include "render/RenderGlobals.h"
using namespace RenderGlobals;

namespace ScalarGlobals {
    int count, colorMethod, fractalType;

    bool isJuliaSet, isInverse;
    bool normalSeed;
    bool invalidPower, normalPower, wholePower, fractionalPower;

    scalar_full_t halfWidth, halfHeight, invWidth, invHeight;
    scalar_full_t realScale, imagScale;

    scalar_full_t point_r, point_i, seed_r, seed_i;
    scalar_full_t N;

    scalar_half_t zoom, aspect;
    scalar_half_t invCount, invLnPow;

    scalar_half_t light_r, light_i, light_h;
    ScalarColor lightColor = DEFAULT_LIGHT_COLOR;

    ScalarSinePalette sinePalette(
        DEFAULT_FREQ_R,
        DEFAULT_FREQ_G,
        DEFAULT_FREQ_B,
        DEFAULT_FREQ_MULT,
        DEFAULT_PHASE_R,
        DEFAULT_PHASE_G,
        DEFAULT_PHASE_B,
        DEFAULT_COS_PHASE
    );
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
        count = iterCount < 1
            ? std::max(MIN_ITERATIONS, count)
            : std::max(MIN_ITERATIONS, iterCount);
        invCount = RECIP_H(count);

        zoom = zoomScale;
        initImageValues();
        return zoomScale > MINIMUM_ZOOM;
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
        wholePower = !normalPower && ISWHOLE_F(N);
        fractionalPower = !wholePower;

        return !invalidPower;
    }

    bool setSinePaletteGlobals(
        scalar_half_t freqR, scalar_half_t freqG, scalar_half_t freqB,
        scalar_half_t totalMult,
        scalar_half_t phaseR, scalar_half_t phaseG, scalar_half_t phaseB,
        scalar_half_t totalPhase
    ) {
        if (ABS_H(totalMult) <= SC_SYM_H(0.0001)) {
            return false;
        }

        sinePalette = ScalarSinePalette(
            freqR,
            freqG,
            freqB,
            totalMult,
            phaseR,
            phaseG,
            phaseB,
            totalPhase
        );

        return true;
    }

    bool setLightGlobals(
        scalar_half_t lr, scalar_half_t li,
        const ScalarColor &color
    ) {
        const scalar_half_t mag = SQRT_H(lr * lr + li * li);

        light_r = lr / mag;
        light_i = li / mag;
        light_h = mag;

        const bool valid =
            color.R >= ZERO_H && color.R <= ONE_H &&
            color.G >= ZERO_H && color.G <= ONE_H &&
            color.B >= ZERO_H && color.B <= ONE_H;
        if (ISNEG0_H(mag) || !valid) return false;

        lightColor = color;
        return true;
    }

    bool setPaletteGlobals(
        const std::vector<ScalarPaletteColor> &entries,
        scalar_half_t totalLength, scalar_half_t offset,
        bool blendEnds
    ) {
        if (entries.size() < 2 || ISNEG0_H(totalLength) || offset < ZERO_H) {
            return false;
        }

        palette = ScalarColorPalette(entries, totalLength, offset, blendEnds);
        return true;
    }
}