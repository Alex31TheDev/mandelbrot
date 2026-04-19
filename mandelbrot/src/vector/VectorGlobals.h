#pragma once
#ifdef USE_VECTORS
#include "CommonDefs.h"

#include "VectorTypes.h"

#include "../scalar/ScalarGlobals.h"

#include "VectorColorPalette.h"
#include "VectorSinePalette.h"

namespace VectorGlobals {
    using namespace ScalarGlobals;

    const simd_full_t f_bailout_vec = SIMD_SET1_F(BAILOUT);

    const simd_half_t h_colorEps_vec = SIMD_SET1_H(COLOR_EPS);
    const simd_half_t h_epsilon_vec = SIMD_SET1_H(HALF_EPSILON);
    const simd_half_t h_invLnBail_vec = SIMD_SET1_H(invLnBail);

    extern simd_full_t f_coordRealScale_vec, f_coordImagScale_vec;
    extern simd_full_t f_coordRealOffset_vec, f_coordImagOffset_vec;

    extern simd_full_t f_seed_r_vec, f_seed_i_vec;
    extern simd_full_t startIterVal;

    extern simd_half_t h_invCount_vec, h_invLnPow_vec;

    extern simd_half_t h_light_r_vec, h_light_i_vec, h_light_h_vec;
    extern simd_half_t h_lightColor_r_vec, h_lightColor_g_vec, h_lightColor_b_vec;

    extern VectorSinePalette sinePalette_vec;
    extern VectorColorPalette palette_vec;

    void initVectors();
}

#else

namespace VectorGlobals {
    [[maybe_unused]] static void initVectors() {}
}

#endif
