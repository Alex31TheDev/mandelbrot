#pragma once

#include <cmath>
#include <immintrin.h>
#include <emmintrin.h>

#include "../scalar/ScalarGlobals.h"

namespace VectorGlobals {
    constexpr int SIMD_WIDTH = 4;

    const __m128i i_zero = _mm_setzero_si128();

    const __m128 f_zero = _mm_set1_ps(0.0f);
    const __m128 f_one = _mm_set1_ps(1.0f);
    const __m128 f_half = _mm_set1_ps(0.5f);
    const __m128 f_invLn2_vec = _mm_set1_ps(ScalarGlobals::invLn2);
    const __m128 f_invLnBail_vec = _mm_set1_ps(ScalarGlobals::invLnBail);
    const __m128 f_pi_2 = _mm_set1_ps((float)M_PI_2);
    const __m128 f_255 = _mm_set1_ps(255.0f);

    const __m256d d_zero = _mm256_set1_pd(0.0);
    const __m256d d_one = _mm256_set1_pd(1.0);
    const __m256d d_bailout_vec = _mm256_set1_pd(ScalarGlobals::BAILOUT);

    extern __m128 f_invCount_vec;
    extern __m128 f_freq_r_vec, f_freq_g_vec, f_freq_b_vec;
    extern __m128 f_light_r_vec, f_light_i_vec, f_light_h_vec;

    void initVectors();
}
