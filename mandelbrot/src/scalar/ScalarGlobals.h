#pragma once

#include "ScalarTypes.h"

#include <cstdint>
#include <cmath>

namespace ScalarGlobals {
    constexpr int MIN_ITERATIONS = 1000;
    constexpr scalar_full_t BAILOUT = CAST_F(256.0);

    constexpr scalar_full_t DEFAULT_POINT_R = CAST_F(0.0);
    constexpr scalar_full_t DEFAULT_POINT_I = CAST_F(0.0);
    constexpr scalar_full_t DEFAULT_SEED_R = CAST_F(0.0);
    constexpr scalar_full_t DEFAULT_SEED_I = CAST_F(0.0);

    constexpr scalar_half_t DEFAULT_PHASE_R = CAST_H(0.0);
    constexpr scalar_half_t DEFAULT_PHASE_G = CAST_H(0.0);
    constexpr scalar_half_t DEFAULT_PHASE_B = CAST_H(0.0);
    constexpr scalar_half_t DEFAULT_COS_PHASE = CAST_H(-M_PI_2);

    constexpr scalar_half_t DEFAULT_FREQ_R = CAST_H(0.98);
    constexpr scalar_half_t DEFAULT_FREQ_G = CAST_H(0.91);
    constexpr scalar_half_t DEFAULT_FREQ_B = CAST_H(0.86);
    constexpr scalar_half_t DEFAULT_FREQ_MULT = CAST_H(0.128);

    constexpr scalar_half_t DEFAULT_LIGHT_R = CAST_H(1.0);
    constexpr scalar_half_t DEFAULT_LIGHT_I = CAST_H(1.0);

    const scalar_half_t invLn2 = CAST_H(1.0 / M_LN2);
    const scalar_half_t invLnBail = CAST_H(RECIP_F(LOG_F(BAILOUT)));

    extern int width, height;
    extern int count, colorMethod;

    extern bool useThreads;
    extern bool isJuliaSet, isInverse;

    extern scalar_full_t halfWidth, halfHeight, invWidth, invHeight;
    extern scalar_full_t point_r, point_i, scale;
    extern scalar_full_t seed_r, seed_i;

    extern scalar_half_t zoom, aspect;
    extern scalar_half_t phase_r, phase_g, phase_b, cosPhase;
    extern scalar_half_t freq_r, freq_g, freq_b, freqMult;
    extern scalar_half_t light_r, light_i, light_h;

    bool setImageGlobals(int img_w, int img_h);
    bool setZoomGlobals(int iterCount, scalar_half_t zoomScale);
    bool setColorGlobals(scalar_half_t R, scalar_half_t G, scalar_half_t B, scalar_half_t mult);
    bool setLightGlobals(scalar_half_t real, scalar_half_t imag);
}
