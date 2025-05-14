#define _USE_MATH_DEFINES
#include <cmath>

#include "GlobalsScalar.h"

const float invLn2 = 1.0f / (float)M_LN2;
const float invLnBail = 1.0f / logf(BAILOUT);

int width, height, colorMethod;
int count;

double half_w, half_h;
double point_r, point_i, scale;

float invCount;
float freqMult, freq_r, freq_g, freq_b;
float light_r, light_i, light_h;