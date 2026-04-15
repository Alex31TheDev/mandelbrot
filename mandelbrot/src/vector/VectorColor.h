#pragma once
#ifdef USE_VECTORS
#include "CommonDefs.h"

#include "VectorTypes.h"

#include "util/InlineUtil.h"

struct VectorColor {
    simd_half_t R, G, B;
};

#define lerp_vec(a, b, t) SIMD_MULADD_H(SIMD_SUB_H(b, a), t, a)

FORCE_INLINE VectorColor colorLerp_vec(
    const VectorColor &a, const VectorColor &b,
    simd_half_t t
) {
    return {
        lerp_vec(a.R, b.R, t),
        lerp_vec(a.G, b.G, t),
        lerp_vec(a.B, b.B, t)
    };
}

#endif
