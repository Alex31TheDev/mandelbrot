#include "ScalarGlobals.h"

#include <cstdlib>
#include <cmath>

namespace ScalarGlobals {
    int width, height, colorMethod;
    int count;

    bool useThreads = false;
    bool isJuliaSet = false, isInverse = false;

    double halfWidth, halfHeight, invWidth, invHeight;
    double point_r = 0, point_i = 0, scale;
    double seed_r = 0, seed_i = 0;

    float zoom, aspect;

    float freq_r, freq_g, freq_b, freqMult;
    float phase_r, phase_g, phase_b, cosPhase = DEFAULT_COS_PHASE;
    float light_r, light_i, light_h;

    bool setImageGlobals(int img_w, int img_h) {
        if (img_w <= 0 || img_h <= 0) return false;
        width = img_w;
        height = img_h;

        aspect = static_cast<float>(width) / height;

        halfWidth = static_cast<double>(width) / 2.0;
        halfHeight = static_cast<double>(height) / 2.0;

        invWidth = 1.0 / static_cast<double>(width);
        invHeight = 1.0 / static_cast<double>(height);

        return true;
    }

    bool setZoomGlobals(int iterCount, float zoomScale) {
        if (iterCount < MIN_ITERATIONS) {
            count = MIN_ITERATIONS;
        } else {
            count = iterCount;
        }

        if (zoomScale < -3.25) return false;
        zoom = zoomScale;

        double zoomPow = pow(10.0, zoom);
        scale = 1.0 / zoomPow;

        if (iterCount == 0) {
            count = MIN_ITERATIONS;

            float visualRange = static_cast<float>(zoomPow) * aspect;
            count += static_cast<int>(powf(log10f(visualRange), 5.0f));
        }

        return true;
    }

    bool setColorGlobals(float R, float G, float B, float mult) {
        phase_r = cosPhase + DEFAULT_PHASE_R;
        phase_g = cosPhase + DEFAULT_PHASE_G;
        phase_b = cosPhase + DEFAULT_PHASE_R;

        if (abs(mult) <= 0.0001f) return false;

        freq_r = R * mult;
        freq_g = G * mult;
        freq_b = B * mult;
        freqMult = mult;

        return true;
    }

    bool setLightGlobals(float real, float imag) {
        float mag = sqrtf(real * real + imag * imag);
        if (mag <= 0) return false;

        light_r = real / mag;
        light_i = imag / mag;
        light_h = mag;

        return true;
    }
}