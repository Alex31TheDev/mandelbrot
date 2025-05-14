#pragma once

#include <immintrin.h>
#include <emmintrin.h>

#define SIMD_WIDTH 4

extern const __m128i i_zero;

extern const __m128 f_zero;
extern const __m128 f_one;
extern const __m128 f_half;
extern const __m128 f_invLn2_vec;
extern const __m128 f_invLnBail_vec;
extern const __m128 f_pi_2;
extern const __m128 f_255;

extern const __m256d d_zero;
extern const __m256d d_one;
extern const __m256d d_bailout_vec;

extern __m128 f_invCount_vec;
extern __m128 f_freq_r_vec, f_freq_g_vec, f_freq_b_vec;
extern __m128 f_light_r_vec, f_light_i_vec, f_light_h_vec;