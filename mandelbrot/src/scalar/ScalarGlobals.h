#pragma once
#include "CommonDefs.h"

#include <vector>

#include "options/FractalTypes.h"
#include "options/ColorMethods.h"

#include "ScalarTypes.h"
#include "util/MathConstants.h"

#include "../image/Image.h"
#include "ScalarColorPalette.h"
#include "ScalarSinePalette.h"

namespace ScalarGlobals {
    constexpr int MIN_ITERATIONS = 500;
    constexpr scalar_full_t BAILOUT = SC_SYM_F(256.0);

    constexpr scalar_full_t DEFAULT_POINT_R = SC_SYM_F(0.0);
    constexpr scalar_full_t DEFAULT_POINT_I = SC_SYM_F(0.0);
    constexpr scalar_full_t DEFAULT_SEED_R = SC_SYM_F(0.0);
    constexpr scalar_full_t DEFAULT_SEED_I = SC_SYM_F(0.0);
    constexpr scalar_full_t DEFAULT_FRACTAL_EXP = SC_SYM_F(2.0);

    constexpr scalar_half_t DEFAULT_FREQ_R = SC_SYM_H(0.98);
    constexpr scalar_half_t DEFAULT_FREQ_G = SC_SYM_H(0.91);
    constexpr scalar_half_t DEFAULT_FREQ_B = SC_SYM_H(0.86);
    constexpr scalar_half_t DEFAULT_FREQ_MULT = SC_SYM_H(0.128);

    constexpr scalar_half_t DEFAULT_PHASE_R = SC_SYM_H(0.0);
    constexpr scalar_half_t DEFAULT_PHASE_G = SC_SYM_H(0.0);
    constexpr scalar_half_t DEFAULT_PHASE_B = SC_SYM_H(0.0);
    constexpr scalar_half_t DEFAULT_COS_PHASE = CAST_H(-M_PI_2);

    constexpr scalar_half_t DEFAULT_LIGHT_R = SC_SYM_H(1.0);
    constexpr scalar_half_t DEFAULT_LIGHT_I = SC_SYM_H(1.0);
    inline constexpr ScalarColor DEFAULT_LIGHT_COLOR{
        SC_SYM_H(1.0),
        SC_SYM_H(1.0),
        SC_SYM_H(1.0)
    };

    const scalar_half_t HALF_EPSILON =
        NEXTAFTER_H(1, 2) - SC_SYM_H(1.0);
    constexpr scalar_half_t COLOR_EPS =
        Image::STRIDE * RECIP_H(SC_SYM_H(255.0));

    const scalar_half_t invLnBail = RECIP_H(LOG_H(BAILOUT));

    extern int count, colorMethod, fractalType;

    extern bool isJuliaSet, isInverse;
    extern bool normalSeed;
    extern bool invalidPower, normalPower, wholePower, fractionalPower;

    extern scalar_full_t halfWidth, halfHeight, invWidth, invHeight;
    extern scalar_full_t realScale, imagScale;

    extern scalar_full_t point_r, point_i, seed_r, seed_i;
    extern scalar_full_t N;

    extern scalar_half_t zoom, aspect;
    extern scalar_half_t invCount, invLnPow;

    extern scalar_half_t light_r, light_i, light_h;
    extern ScalarColor lightColor;

    extern ScalarSinePalette sinePalette;
    extern ScalarColorPalette palette;

    inline bool useQuadPath() {
        return fractalType == 0 && normalPower && !isJuliaSet && !isInverse;
    }

    void initImageValues();
    bool setZoomGlobals(int iterCount = 0, scalar_half_t zoomScale = 0);
    bool setZoomPoints(
        scalar_full_t pr = DEFAULT_POINT_R, scalar_full_t pi = DEFAULT_POINT_I,
        scalar_full_t sr = DEFAULT_SEED_R, scalar_full_t si = DEFAULT_SEED_I
    );
    bool setFractalType(
        int type = FractalTypes::DEFAULT_FRACTAL_TYPE.id,
        bool julia = false, bool inverse = false
    );
    bool setColorMethod(int method = ColorMethods::DEFAULT_COLOR_METHOD.id);
    bool setFractalExponent(scalar_full_t pw = DEFAULT_FRACTAL_EXP);
    bool setSinePaletteGlobals(
        scalar_half_t freqR = DEFAULT_FREQ_R,
        scalar_half_t freqG = DEFAULT_FREQ_G,
        scalar_half_t freqB = DEFAULT_FREQ_B,
        scalar_half_t totalMult = DEFAULT_FREQ_MULT,
        scalar_half_t phaseR = DEFAULT_PHASE_R,
        scalar_half_t phaseG = DEFAULT_PHASE_G,
        scalar_half_t phaseB = DEFAULT_PHASE_B,
        scalar_half_t totalPhase = DEFAULT_COS_PHASE
    );
    bool setLightGlobals(
        scalar_half_t lr = DEFAULT_LIGHT_R, scalar_half_t li = DEFAULT_LIGHT_I,
        const ScalarColor &color = DEFAULT_LIGHT_COLOR
    );
    bool setPaletteGlobals(
        const std::vector<ScalarPaletteColor> &entries,
        scalar_half_t totalLength = SC_SYM_H(10.0),
        scalar_half_t offset = ZERO_H,
        bool blendEnds = true
    );

    [[maybe_unused]] static void setAllDefaults() {
        setZoomGlobals();
        setZoomPoints();
        setFractalExponent();
        setFractalType();
        setColorMethod();
        setSinePaletteGlobals();
        setLightGlobals();
    }
}
