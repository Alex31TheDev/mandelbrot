#include "ScalarGlobals.h"

#include <cstdlib>
#include <cmath>

namespace ScalarGlobals {
    int width, height, colorMethod;
    int count;

    double half_w, half_h;
    double point_r, point_i, scale;

    float zoom, aspect, invCount;
    float freqMult, freq_r, freq_g, freq_b;
    float light_r, light_i, light_h;

    bool setImageGlobals(int img_w, int img_h) {
        if (img_w <= 0 || img_h <= 0) return false;
        width = img_w;
        height = img_h;

        aspect = (float)width / (float)height;

        half_w = (double)width * 0.5;
        half_h = (double)height * 0.5;

        return true;
    }

    bool setZoomGlobals(float zoomScale) {
        if (zoomScale <= 0) return false;
        zoom = zoomScale;

        count = (int)(powf(1.3f, zoom) + 22.0f * zoom);
        invCount = 1.0f / (float)count;
        scale = 1.0 / pow(2.0, (double)zoom);

        return true;
    }

    bool setColorGlobals(float R, float G, float B, float mult) {
        if (abs(mult) <= 0.0001f) return false;

        freq_r = R * mult;
        freq_g = G * mult;
        freq_b = B * mult;

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