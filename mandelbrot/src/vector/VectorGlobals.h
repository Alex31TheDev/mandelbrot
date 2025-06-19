#pragma once
#ifdef USE_VECTORS
#include "CommonDefs.h"

#include "VectorTypes.h"
#include "../scalar/ScalarTypes.h"

#include "../util/MathConstants.h"
#include "../scalar/ScalarGlobals.h"

#include "VectorColorPalette.h"

namespace VectorGlobals {
    using namespace ScalarGlobals;

    const simd_full_t f_zero = SIMD_SET_F(0.0);
    const simd_full_t f_one = SIMD_SET_F(1.0);
    const simd_full_t f_neg_one = SIMD_SET_F(-1.0);
    const simd_full_t f_bailout_vec = SIMD_SET_F(BAILOUT);

    const simd_half_t h_zero = SIMD_SET_H(0.0);
    const simd_half_t h_one = SIMD_SET_H(1.0);
    const simd_half_t h_half = SIMD_SET_H(0.5);
    const simd_half_t h_invLnBail_vec = SIMD_SET_H(invLnBail);
    const simd_half_t h_pi_2 = SIMD_SET_H(M_PI_2);
    const simd_half_t h_255 = SIMD_SET_H(255.0);
    const simd_half_t h_colorEps_vec = SIMD_SET_H(COLOR_EPS);
    const simd_half_t h_epsilon_vec = SIMD_SET_H(HALF_EPSILON);

    const simd_half_int_t hi_zero = SIMD_SET_INT_H(0);
    const simd_half_int_t hi_one = SIMD_SET_INT_H(1);

    extern simd_full_t f_idx_vec;
    extern simd_full_t f_halfWidth_vec, f_invWidth_vec;

    extern simd_full_t f_realScale_vec, f_point_r_vec;
    extern simd_full_t f_seed_r_vec, f_seed_i_vec;

    extern simd_half_t h_invCount_vec, h_invLnPow_vec;

    extern simd_half_t h_freq_r_vec, h_freq_g_vec, h_freq_b_vec;
    extern simd_half_t h_phase_r_vec, h_phase_g_vec, h_phase_b_vec;
    extern simd_half_t h_light_r_vec, h_light_i_vec, h_light_h_vec;

    void initVectors();

    const VectorColorPalette palette_vec(palette);
}

#else

namespace VectorGlobals {
    [[maybe_unused]] static void initVectors() {}
}

#endif