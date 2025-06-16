#pragma once
#ifdef USE_VECTORS

#include "VectorTypes.h"

#include "VectorGlobals.h"

#include "../util/InlineUtil.h"

FORCE_INLINE simd_full_t getCenterReal_vec(int width, int x) {
    using namespace VectorGlobals;

    const simd_full_t width_vec = SIMD_SET_F(width);
    const simd_full_t x_vec = SIMD_SET_F(x);

    const simd_full_t offset = SIMD_ADD_F(x_vec, f_idx_vec);
    const simd_full_t center = SIMD_SUB_F(offset, f_halfWidth_vec);
    const simd_full_t normal = SIMD_MUL_F(center, f_invWidth_vec);
    const simd_full_t scaled = SIMD_MUL_F(normal, f_realScale_vec);

    const simd_full_t vals = SIMD_ADD_F(scaled, f_point_r_vec);

    const simd_full_mask_t valid = SIMD_CMP_LT_F(f_idx_vec, width_vec);
    return SIMD_BLEND_F(f_bailout_vec, vals, valid);
}

#endif