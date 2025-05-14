#pragma once

#define BAILOUT 256.0

#define DEFAULT_FREQ_R 1.0f
#define DEFAULT_FREQ_G 0.5f
#define DEFAULT_FREQ_B 0.12f
#define DEFAULT_FREQ_MULT 64

#define DEFAULT_LIGHT_R 1.0f
#define DEFAULT_LIGHT_I 1.0f

extern const float invLn2;
extern const float invLnBail;

extern int width, height, colorMethod;
extern int count;

extern double half_w, half_h;
extern double point_r, point_i, scale;

extern float invCount;
extern float freqMult, freq_r, freq_g, freq_b;
extern float light_r, light_i, light_h;
