#pragma once
#ifdef USE_VECTORS

#include <vector>

#include "../scalar/ScalarTypes.h"
#include "VectorTypes.h"

#include "../scalar/ScalarColorPalette.h"

class VectorColorPalette {
public:
    static inline simd_half_t lerp_vec(
        simd_half_t a, simd_half_t b, simd_half_t t
    ) {
        return SIMD_MULADD_H(SIMD_SUB_H(b, a), t, a);
    }

    VectorColorPalette(const ScalarColorPalette &palette);

    void sampleSIMD(const simd_half_t &x,
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

    _SIMDSegment _locate_vec(const simd_half_t &x) const;
};

#endif
