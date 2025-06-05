#ifdef USE_VECTORS
#include "VectorGlobals.h"

#include <array>

#include "VectorTypes.h"
#include "../scalar/ScalarTypes.h"

#include "../scalar/ScalarGlobals.h"
using namespace ScalarGlobals;

static auto makeIndexArray() {
    std::array<scalar_full_t, SIMD_FULL_WIDTH> arr{};

    for (int i = 0; i < SIMD_FULL_WIDTH; i++) {
        arr[i] = CAST_F(i);
    }

    return arr;
}

namespace VectorGlobals {
    simd_full_t f_idx_vec;
    simd_full_t f_halfWidth_vec, f_invWidth_vec;

    simd_full_t f_realScale_vec, f_point_r_vec;
    simd_full_t f_seed_r_vec, f_seed_i_vec;

    simd_half_t h_invCount_vec, h_invLnPow_vec;

    simd_half_t h_freq_r_vec, h_freq_g_vec, h_freq_b_vec;
    simd_half_t h_phase_r_vec, h_phase_g_vec, h_phase_b_vec;
    simd_half_t h_light_r_vec, h_light_i_vec, h_light_h_vec;

    void initVectors() {
        f_idx_vec = SIMD_LOAD_F(makeIndexArray().data());

        f_halfWidth_vec = SIMD_SET_F(halfWidth);
        f_invWidth_vec = SIMD_SET_F(invWidth);

        f_realScale_vec = SIMD_SET_F(realScale);
        f_point_r_vec = SIMD_SET_F(point_r);

        f_seed_r_vec = SIMD_SET_F(seed_r);
        f_seed_i_vec = SIMD_SET_F(seed_i);

        h_invCount_vec = SIMD_SET_H(invCount);
        h_invLnPow_vec = SIMD_SET_H(invLnPow);

        h_freq_r_vec = SIMD_SET_H(freq_r);
        h_freq_g_vec = SIMD_SET_H(freq_g);
        h_freq_b_vec = SIMD_SET_H(freq_b);

        h_phase_r_vec = SIMD_SET_H(phase_r);
        h_phase_g_vec = SIMD_SET_H(phase_g);
        h_phase_b_vec = SIMD_SET_H(phase_b);

        h_light_r_vec = SIMD_SET_H(light_r);
        h_light_i_vec = SIMD_SET_H(light_i);
        h_light_h_vec = SIMD_SET_H(light_h);
    }
}

#endif