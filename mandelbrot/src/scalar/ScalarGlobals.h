#pragma once

#include <cmath>

namespace ScalarGlobals {
    constexpr double BAILOUT = 256.0;

    constexpr double DEFAULT_SEED_R = 0.0;
    constexpr double DEFAULT_SEED_I = 0.0;

    constexpr float DEFAULT_FREQ_R = 1.0f;
    constexpr float DEFAULT_FREQ_G = 0.5f;
    constexpr float DEFAULT_FREQ_B = 0.12f;
    constexpr float DEFAULT_FREQ_MULT = 64.0f;

    constexpr float DEFAULT_LIGHT_R = 1.0f;
    constexpr float DEFAULT_LIGHT_I = 1.0f;

    const float invLn2 = 1.0f / (float)M_LN2;
    const float invLnBail = 1.0f / logf(BAILOUT);

    extern int width, height, colorMethod;
    extern int count;

    extern bool isJuliaSet, isInverse;

    extern double half_w, half_h;
    extern double point_r, point_i, scale;
    extern double seed_r, seed_i;

    extern float zoom, aspect, invCount;
    extern float freqMult, freq_r, freq_g, freq_b;
    extern float light_r, light_i, light_h;

    bool setImageGlobals(int img_w, int img_h);
    bool setZoomGlobals(float zoomScale);
    bool setColorGlobals(float R, float G, float B, float mult);
    bool setLightGlobals(float real, float imag);
}
