#pragma once

#include <cmath>

namespace ScalarGlobals {
    constexpr int MIN_ITERATIONS = 1000;
    constexpr double BAILOUT = 256.0;

    constexpr double DEFAULT_SEED_R = 0.0;
    constexpr double DEFAULT_SEED_I = 0.0;

    constexpr float DEFAULT_PHASE_R = 0.0f;
    constexpr float DEFAULT_PHASE_G = 0.0f;
    constexpr float DEFAULT_PHASE_B = 0.0f;
    constexpr float DEFAULT_COS_PHASE = -static_cast<float>(M_PI) / 2.0f;

    constexpr float DEFAULT_FREQ_R = 0.98f;
    constexpr float DEFAULT_FREQ_G = 0.91f;
    constexpr float DEFAULT_FREQ_B = 0.86f;
    constexpr float DEFAULT_FREQ_MULT = 0.128f;

    constexpr float DEFAULT_LIGHT_R = 1.0f;
    constexpr float DEFAULT_LIGHT_I = 1.0f;

    const float invLn2 = 1.0f / static_cast<float>(M_LN2);
    const float invLnBail = 1.0f / logf(BAILOUT);

    extern int width, height, colorMethod;
    extern int count;

    extern bool useThreads;
    extern bool isJuliaSet, isInverse;

    extern double halfWidth, halfHeight, invWidth, invHeight;
    extern double point_r, point_i, scale;
    extern double seed_r, seed_i;

    extern float zoom, aspect;
    extern float phase_r, phase_g, phase_b, cosPhase;
    extern float freq_r, freq_g, freq_b, freqMult;
    extern float light_r, light_i, light_h;

    bool setImageGlobals(int img_w, int img_h);
    bool setZoomGlobals(int iterCount, float zoomScale);
    bool setColorGlobals(float R, float G, float B, float mult);
    bool setLightGlobals(float real, float imag);
}
