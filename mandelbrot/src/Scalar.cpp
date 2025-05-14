#include <cstdint>
#define _USE_MATH_DEFINES
#include <cmath>

#include "Scalar.h"
#include "GlobalsScalar.h"

inline float normCos(float x) {
    return (cosf(x - M_PI_2) + 1.0f) * 0.5f;
}

inline void getColorPixel(float val, float &outR, float &outG, float &outB) {
    float R_x = val * freq_r;
    float G_x = val * freq_g;
    float B_x = val * freq_b;

    outR = normCos(R_x);
    outG = normCos(G_x);
    outB = normCos(B_x);
}

inline uint8_t pixelToInt(float val) {
    return (uint8_t)(val * 255.0f);
}

inline void setPixel(uint8_t *pixels, int &pos, float R, float G, float B) {
    uint8_t R_i = pixelToInt(R);
    uint8_t G_i = pixelToInt(G);
    uint8_t B_i = pixelToInt(B);

    pixels[pos++] = R_i;
    pixels[pos++] = G_i;
    pixels[pos++] = B_i;
}

float getSmoothIterVal(int i, float mag) {
    float smooth = i - logf(logf(sqrtf(mag)) * invLnBail) * invLn2;
    return smooth * invCount;
}

float getLightVal(double zr, double zi, double dr, double di) {
    float dsum = 1.0f / (float)(dr * dr + di * di);
    float ur = (float)(zr * dr + zi * di) * dsum;
    float ui = (float)(zi * dr - zr * di) * dsum;

    float umag = 1.0f / sqrtf(ur * ur + ui * ui);
    ur *= umag;
    ui *= umag;

    float light = (ur * light_r + ui * light_i + light_h) / (light_h + 1.0f);
    return fmaxf(light, 0.0f);
}

void renderPixelScalar(uint8_t *pixels, int &pos, int x, double ci) {
    double cr = (x - half_w) * scale + point_r;

    double zr = 0.0, zi = 0.0;
    double dr = 1.0, di = 0.0;

    int i = 0;
    double mag = 0;

    for (; i < count; i++) {
        double zr2 = zr * zr;
        double zi2 = zi * zi;
        mag = zr2 + zi2;

        if (mag > BAILOUT) break;

        switch (colorMethod) {
            case 1:
            {
                double dr2 = dr;
                dr = 2.0 * (zr * dr - zi * di) + 1.0;
                di = 2.0 * (zr * di + zi * dr2);
            }
            break;
        }

        zi = 2.0 * zr * zi - ci;
        zr = zr2 - zi2 + cr;
    }

    if (i == count) {
        setPixel(pixels, pos, 0.0f, 0.0f, 0.0f);
        return;
    }

    switch (colorMethod) {
        case 0:
        {
            float val = getSmoothIterVal(i, mag);

            float R, G, B;
            getColorPixel(val, R, G, B);

            setPixel(pixels, pos, R, G, B);
        }
        break;

        case 1:
        {
            float val = getLightVal(zr, zi, dr, di);
            setPixel(pixels, pos, val, val, val);
        }
        break;
    }
}