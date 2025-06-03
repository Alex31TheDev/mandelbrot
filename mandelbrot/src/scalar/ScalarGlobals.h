#pragma once

#include <cmath>

#include "ScalarTypes.h"

namespace ScalarGlobals {
    constexpr int MIN_ITERATIONS = 500;
    constexpr scalar_full_t BAILOUT = SC_SYM_F(256.0);

    constexpr scalar_full_t DEFAULT_POINT_R = SC_SYM_F(0.0);
    constexpr scalar_full_t DEFAULT_POINT_I = SC_SYM_F(0.0);
    constexpr scalar_full_t DEFAULT_SEED_R = SC_SYM_F(0.0);
    constexpr scalar_full_t DEFAULT_SEED_I = SC_SYM_F(0.0);
    constexpr scalar_full_t DEFAULT_FRACTAL_EXP = SC_SYM_F(2.0);

    constexpr scalar_half_t DEFAULT_PHASE_R = SC_SYM_H(0.0);
    constexpr scalar_half_t DEFAULT_PHASE_G = SC_SYM_H(0.0);
    constexpr scalar_half_t DEFAULT_PHASE_B = SC_SYM_H(0.0);
    constexpr scalar_half_t DEFAULT_COS_PHASE = SC_SYM_H(-M_PI_2);

    constexpr scalar_half_t DEFAULT_FREQ_R = SC_SYM_H(0.98);
    constexpr scalar_half_t DEFAULT_FREQ_G = SC_SYM_H(0.91);
    constexpr scalar_half_t DEFAULT_FREQ_B = SC_SYM_H(0.86);
    constexpr scalar_half_t DEFAULT_FREQ_MULT = SC_SYM_H(0.128);

    constexpr scalar_half_t DEFAULT_LIGHT_R = SC_SYM_H(1.0);
    constexpr scalar_half_t DEFAULT_LIGHT_I = SC_SYM_H(1.0);

    const scalar_half_t invLnBail = RECIP_H(LOG_H(BAILOUT));

    extern int width, height;
    extern int count, colorMethod;

    extern bool useThreads;
    extern bool isJuliaSet, isInverse;

    extern scalar_full_t halfWidth, halfHeight, invWidth, invHeight;
    extern scalar_full_t realScale, imagScale;

    extern scalar_full_t point_r, point_i;
    extern scalar_full_t seed_r, seed_i;
    extern scalar_full_t N;

    extern scalar_half_t zoom, aspect;
    extern scalar_half_t invCount, invLnPow;

    extern scalar_half_t freq_r, freq_g, freq_b, freqMult;
    extern scalar_half_t phase_r, phase_g, phase_b, cosPhase;
    extern scalar_half_t light_r, light_i, light_h;

    const bool setImageGlobals(const int img_w, const int img_h);
    const bool setZoomGlobals(const int iterCount, const scalar_half_t zoomScale);
    const bool setFractalExponent(const scalar_full_t pw);
    const bool setColorGlobals(const scalar_half_t R, const scalar_half_t G, const scalar_half_t B,
        const scalar_half_t mult);
    const bool setLightGlobals(const scalar_half_t real, const scalar_half_t imag);
}
