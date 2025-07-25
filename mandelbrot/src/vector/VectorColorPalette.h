#pragma once
#ifdef USE_VECTORS
#include "CommonDefs.h"

#include <vector>

#include "../scalar/ScalarTypes.h"
#include "VectorTypes.h"
#include "VectorColor.h"

#include "../scalar/ScalarColorPalette.h"

#include "../util/InlineUtil.h"

class VectorColorPalette {
public:
    static FORCE_INLINE simd_half_t lerp_vec(
        simd_half_t a, simd_half_t b, simd_half_t t
    ) {
        return SIMD_MULADD_H(SIMD_SUB_H(b, a), t, a);
    }

    static FORCE_INLINE VectorColor lerp_vec(
        const VectorColor &a, const VectorColor &b,
        simd_half_t t
    ) {
        return {
            lerp_vec(a.R, b.R, t),
            lerp_vec(a.G, b.G, t),
            lerp_vec(a.B, b.B, t)
        };
    }

    explicit VectorColorPalette(const ScalarColorPalette &palette);

    VectorColor VECTOR_CALL sampleSIMD(simd_half_t x) const;

    void VECTOR_CALL sampleSIMD(simd_half_t x,
        simd_half_t &outR, simd_half_t &outG, simd_half_t &outB) const;

private:
    size_t _n, _numSegments;
    bool _blendEnds;
    simd_half_t _totalLength_vec, _invLength_vec;

    simd_half_t _offset_vec, _epsilon_vec;
    simd_half_int_t _n_min1_vec;

    std::vector<simd_half_t> _accum_vec;
    std::vector<scalar_half_t> _accum, _inv;
    std::vector<scalar_half_t> _R, _G, _B;

    struct _SIMDSegment {
        simd_half_int_t idx, next;
        simd_half_t u;
    };
    _SIMDSegment VECTOR_CALL _locate_vec(simd_half_t x) const;
};

#endif
