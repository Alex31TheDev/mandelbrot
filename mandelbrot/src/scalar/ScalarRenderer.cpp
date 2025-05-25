#include "ScalarRenderer.h"

#include <cstdint>
#include <cmath>

#include "ScalarGlobals.h"
using namespace ScalarGlobals;

#include "ScalarCoords.h"

static inline float normCos(float x) {
    return (cosf(x) + 1.0f) * 0.5f;
}

static inline void getColorPixel(float val,
    float &outR, float &outG, float &outB) {
    float R_x = phase_r + val * freq_r;
    float G_x = phase_g + val * freq_g;
    float B_x = phase_b + val * freq_b;

    outR = normCos(R_x);
    outG = normCos(G_x);
    outB = normCos(B_x);
}

static inline uint8_t pixelToInt(float val) {
    return static_cast<uint8_t>(val * 255.0f);
}

static inline float getSmoothIterVal(int i, float mag) {
    float sqrt_mag = sqrtf(mag);
    float m = logf(logf(sqrt_mag) * invLnBail) * invLn2;
    return static_cast<float>(i) - m;
}

static inline float getLightVal(double zr, double zi, double dr, double di) {
    float dsum = 1.0f / static_cast<float>(dr * dr + di * di);
    float ur = static_cast<float>(zr * dr + zi * di) * dsum;
    float ui = static_cast<float>(zi * dr - zr * di) * dsum;

    float umag = 1.0f / sqrtf(ur * ur + ui * ui);
    ur *= umag;
    ui *= umag;

    float light = (ur * light_r + ui * light_i + light_h) / (light_h + 1.0f);
    return fmaxf(light, 0.0f);
}

namespace ScalarRenderer {
    inline void setPixel(uint8_t *pixels, int &pos,
        float R, float G, float B) {
        pixels[pos++] = pixelToInt(R);
        pixels[pos++] = pixelToInt(G);
        pixels[pos++] = pixelToInt(B);
    }

    void renderPixelScalar(uint8_t *pixels, int &pos,
        int x, double ci) {
        double cr = getCenterReal(x);

        if (isInverse) {
            double cmag = cr * cr + ci * ci;

            if (cmag != 0) {
                cr = cr / cmag;
                ci = -ci / cmag;
            }
        }

        double zr, zi;
        double dr = 1.0, di = 0.0;

        if (isJuliaSet) {
            zr = cr;
            zi = ci;

            cr = seed_r;
            ci = seed_i;
        } else {
            zr = seed_r;
            zi = seed_i;
        }

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

            zi = 2.0 * zr * zi + ci;
            zr = zr2 - zi2 + cr;
        }

        if (i == count) {
            setPixel(pixels, pos, 0.0f, 0.0f, 0.0f);
            return;
        }

        switch (colorMethod) {
            case 0:
            {
                float val = getSmoothIterVal(i, static_cast<float>(mag));

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
}