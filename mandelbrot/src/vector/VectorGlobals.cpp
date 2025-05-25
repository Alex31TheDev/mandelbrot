#include "VectorGlobals.h"

#include <immintrin.h>

#include "../scalar/ScalarGlobals.h"
using namespace ScalarGlobals;

namespace VectorGlobals {
    __m256d d_idx_vec;
    __m256d d_halfWidth_vec, d_invWidth_vec;

    __m256d d_scale_vec, d_point_r_vec;
    __m256d d_seed_r_vec, d_seed_i_vec;

    __m128 f_phase_r_vec, f_phase_g_vec, f_phase_b_vec;
    __m128 f_freq_r_vec, f_freq_g_vec, f_freq_b_vec;
    __m128 f_light_r_vec, f_light_i_vec, f_light_h_vec;

    void initVectors() {
        double idx_arr[4] = { 0 };

        for (int i = 0; i < SIMD_WIDTH; i++) {
            idx_arr[i] = static_cast<double>(i);
        }

        d_idx_vec = _mm256_load_pd(idx_arr);

        d_halfWidth_vec = _mm256_set1_pd(halfWidth);
        d_invWidth_vec = _mm256_set1_pd(invWidth);
        d_scale_vec = _mm256_set1_pd(scale);

        d_point_r_vec = _mm256_set1_pd(point_r);

        d_seed_r_vec = _mm256_set1_pd(seed_r);
        d_seed_i_vec = _mm256_set1_pd(seed_i);

        f_phase_r_vec = _mm_set1_ps(phase_r);
        f_phase_g_vec = _mm_set1_ps(phase_g);
        f_phase_b_vec = _mm_set1_ps(phase_b);

        f_freq_r_vec = _mm_set1_ps(freq_r);
        f_freq_g_vec = _mm_set1_ps(freq_g);
        f_freq_b_vec = _mm_set1_ps(freq_b);

        f_light_r_vec = _mm_set1_ps(light_r);
        f_light_i_vec = _mm_set1_ps(light_i);
        f_light_h_vec = _mm_set1_ps(light_h);
    }
}