#pragma once
#ifdef USE_VECTORS
#include "CommonDefs.h"

#include "VectorTypes.h"
#include "VectorGlobals.h"

#include "../util/InlineUtil.h"

FORCE_INLINE simd_full_t getCenterReal_vec(int width, int x) {
    using namespace VectorGlobals;

    const simd_full_t offset = SIMD_ADD_F(SIMD_SET1_F(x), f_idx_vec);

    const simd_full_t normed = SIMD_MUL_F(
        SIMD_SUB_F(offset, f_halfWidth_vec),
        f_invWidth_vec
    );

    const simd_full_t vals = SIMD_MULADD_F(
        normed,
        f_realScale_vec, f_point_r_vec
    );

    const simd_full_mask_t valid = SIMD_CMP_LT_F(
        f_idx_vec, SIMD_SET1_F(width)
    );

    return SIMD_BLEND_F(f_bailout_vec, vals, valid);
}

#endif