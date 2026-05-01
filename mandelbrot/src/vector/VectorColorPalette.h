#pragma once
#ifdef USE_VECTORS
#include "CommonDefs.h"

#include <vector>

#include "scalar/ScalarTypes.h"
#include "VectorTypes.h"
#include "VectorColor.h"

#include "scalar/ScalarColorPalette.h"

#include "util/InlineUtil.h"

class VectorColorPalette {
public:
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
