#include "VectorGlobals.h"

#include <xmmintrin.h>

#include "../scalar/ScalarGlobals.h"
using namespace ScalarGlobals;

namespace VectorGlobals {
    __m128 f_invCount_vec;
    __m128 f_freq_r_vec, f_freq_g_vec, f_freq_b_vec;
    __m128 f_light_r_vec, f_light_i_vec, f_light_h_vec;

    void initVectors() {
        f_invCount_vec = _mm_set1_ps(invCount);

        f_freq_r_vec = _mm_set1_ps(freq_r);
        f_freq_g_vec = _mm_set1_ps(freq_g);
        f_freq_b_vec = _mm_set1_ps(freq_b);

        f_light_r_vec = _mm_set1_ps(light_r);
        f_light_i_vec = _mm_set1_ps(light_i);
        f_light_h_vec = _mm_set1_ps(light_h);
    }
}