#ifdef USE_VECTORS
#include "CommonDefs.h"
#include "VectorColorPalette.h"

#include <iterator>
#include <algorithm>

#include "VectorTypes.h"
#include "VectorColor.h"

#include "../scalar/ScalarTypes.h"
#include "../scalar/ScalarColorPalette.h"

#include "../util/InlineUtil.h"

VectorColorPalette::VectorColorPalette(const ScalarColorPalette &palette) {
    _n = palette._colors.size();
    _numSegments = palette._numSegments;
    _blendEnds = palette._blendEnds;

    _totalLength_vec = SIMD_SET1_H(palette._totalLength);
    _invLength_vec = SIMD_SET1_H(palette._invLength);

    _offset_vec = SIMD_SET1_H(palette._offset);
    _epsilon_vec = SIMD_SET1_H(palette._epsilon);
    _n_min1_vec = SIMD_SET1_INT_H(_n - 1);

    std::ranges::transform(palette._accum, std::back_inserter(_accum_vec),
        [](scalar_half_t x) { return SIMD_SET1_H(x); });

    _accum = palette._accum;
    _inv = palette._inv;

    _R.resize(_n);
    _G.resize(_n);
    _B.resize(_n);

    for (size_t i = 0; i < _n; ++i) {
        _R[i] = palette._colors[i].R;
        _G[i] = palette._colors[i].G;
        _B[i] = palette._colors[i].B;
    }
}

FORCE_INLINE VectorColorPalette::_SIMDSegment VECTOR_CALL
VectorColorPalette::_locate_vec(simd_half_t x) const {
    const simd_half_t inVal = SIMD_ADD_H(x, _offset_vec);
    simd_half_t t = SIMD_MUL_H(inVal, _invLength_vec);
    t = SIMD_SUBMUL_H(_totalLength_vec, SIMD_FLOOR_H(t), inVal);

    const simd_half_mask_t negative = SIMD_ISNEG_H(t);
    t = SIMD_ADD_MASK_H(t, _totalLength_vec, negative);
    t = SIMD_MIN_H(t, _epsilon_vec);

    simd_half_int_t idx = SIMD_ZERO_HI;

    for (size_t i = 0; i + 1 < _numSegments; i++) {
        const simd_half_mask_t matched = SIMD_CMP_LE_H(_accum_vec[i + 1], t);

        idx = SIMD_ADD_INT_MASK_H(
            idx, SIMD_ONE_HI,
            SIMD_HALF_MASK_TO_INT_MASK(matched)
        );
    }

    const simd_half_t accum = SIMD_GATHER_H(_accum.data(), idx);
    const simd_half_t inv = SIMD_GATHER_H(_inv.data(), idx);
    const simd_half_t u = SIMD_MUL_H(SIMD_SUB_H(t, accum), inv);

    simd_half_int_t next = SIMD_ADD_INT_H(idx, SIMD_ONE_HI);

    if (_blendEnds) {
        const simd_half_int_mask_t wrapMask =
            SIMD_CMP_INT_GT_H(next, _n_min1_vec);
        next = SIMD_BLEND_INT8_H(next, SIMD_ZERO_HI, wrapMask);
    }

    return { idx, next, u };
}

VectorColor VECTOR_CALL
VectorColorPalette::sampleSIMD(simd_half_t x) const {
    if (_n == 0) {
        return {
            SIMD_ZERO_H,
            SIMD_ZERO_H,
            SIMD_ZERO_H
        };
    }

    const _SIMDSegment seg = _locate_vec(x);

    const float *R = _R.data();
    const float *G = _G.data();
    const float *B = _B.data();

    const VectorColor color1 = {
        SIMD_GATHER_H(R, seg.idx),
        SIMD_GATHER_H(G, seg.idx),
        SIMD_GATHER_H(B, seg.idx),
    };

    const VectorColor color2 = {
        SIMD_GATHER_H(R, seg.next),
        SIMD_GATHER_H(G, seg.next),
        SIMD_GATHER_H(B, seg.next),
    };

    return lerp_vec(color1, color2, seg.u);
}

void VECTOR_CALL VectorColorPalette::sampleSIMD(
    simd_half_t x,
    simd_half_t &outR, simd_half_t &outG, simd_half_t &outB
) const {
    if (_n == 0) {
        outR = outG = outB = SIMD_ZERO_H;
        return;
    }

    const _SIMDSegment seg = _locate_vec(x);

    const float *R = _R.data();
    const float *G = _G.data();
    const float *B = _B.data();

    outR = lerp_vec(
        SIMD_GATHER_H(R, seg.idx),
        SIMD_GATHER_H(R, seg.next),
        seg.u
    );

    outG = lerp_vec(
        SIMD_GATHER_H(G, seg.idx),
        SIMD_GATHER_H(G, seg.next),
        seg.u
    );

    outB = lerp_vec(
        SIMD_GATHER_H(B, seg.idx),
        SIMD_GATHER_H(B, seg.next),
        seg.u
    );
}

#endif