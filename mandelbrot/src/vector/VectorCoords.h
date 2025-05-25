#pragma once

#include "VectorGlobals.h"

#include <immintrin.h>

static inline __m256d getCenterReal(int width, int x) {
    using namespace VectorGlobals;

    __m256d width_vec = _mm256_set1_pd(static_cast<double>(width));
    __m256d x_vec = _mm256_set1_pd(static_cast<double>(x));

    __m256d offset = _mm256_add_pd(x_vec, d_idx_vec);
    __m256d center = _mm256_sub_pd(offset, d_halfWidth_vec);
    __m256d normal = _mm256_mul_pd(center, d_invWidth_vec);
    __m256d scaled = _mm256_mul_pd(normal, d_scale_vec);

    __m256d vals = _mm256_add_pd(scaled, d_point_r_vec);
    __m256d mask = _mm256_cmp_pd(d_idx_vec, width_vec, _CMP_LT_OQ);

    return _mm256_blendv_pd(d_bailout_vec, vals, mask);
}
