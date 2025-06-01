#ifdef USE_VECTORS
#include "VectorCoords.h"

#include <immintrin.h>

#include "VectorTypes.h"

#include "VectorGlobals.h"
using namespace VectorGlobals;

simd_full_t getCenterReal(int width, int x) {
    simd_full_t width_vec = SIMD_SET_F(width);
    simd_full_t x_vec = SIMD_SET_F(x);

    simd_full_t offset = SIMD_ADD_F(x_vec, f_idx_vec);
    simd_full_t center = SIMD_SUB_F(offset, f_halfWidth_vec);
    simd_full_t normal = SIMD_MUL_F(center, f_invWidth_vec);
    simd_full_t scaled = SIMD_MUL_F(normal, f_realScale_vec);

    simd_full_t vals = SIMD_ADD_F(scaled, f_point_r_vec);

    simd_full_mask_t mask = SIMD_CMP_LT_F(f_idx_vec, width_vec);
    return SIMD_BLEND_F(f_bailout_vec, vals, mask);
}

#endif