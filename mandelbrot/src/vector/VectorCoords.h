#pragma once
#ifdef USE_VECTORS
#include "CommonDefs.h"

#include <array>
#include <utility>

#include "../scalar/ScalarTypes.h"
#include "VectorTypes.h"

#include "VectorGlobals.h"

#include "../util/InlineUtil.h"
#include "../util/AssertUtil.h"

template<size_t N>
consteval auto _makeIndexArray() {
    return[]<std::size_t... Is>(std::index_sequence<Is...>) {
        return std::array<std::size_t, N + 1>{ { Is..., N } };
    }(std::make_index_sequence<N>{});
}

template <int x>
auto _makeValidMasks() {
    static_assert(x > 0 && x <= 32, "x must be between 1 and 32");
    std::array<simd_full_mask_t, x> masks{};

    for (int i = 0; i < x; i++) {
        const int bitmask = (1ULL << (i + 1)) - 1;
        masks[i] = SIMD_MASK_FROMBITS_F(bitmask);
    }

    return masks;
}

constexpr auto _idx_arr = _makeIndexArray<SIMD_FULL_WIDTH>();
constexpr auto _full_arr = convertToFull(_idx_arr);

static_assert(is_constexpr_test<_idx_arr>());
static_assert(is_constexpr_test<_full_arr>());

const simd_full_t _f_idx_vec = SIMD_LOADU_F(_full_arr.data());

const auto _validMasks = _makeValidMasks<SIMD_FULL_WIDTH>();

FORCE_INLINE simd_full_t getCenterReal_vec(simd_full_t x) {
    return SIMD_MULADD_F(
        x,
        VectorGlobals::f_coordRealScale_vec,
        VectorGlobals::f_coordRealOffset_vec
    );
}

FORCE_INLINE simd_full_t getCenterImag_vec(simd_full_t y) {
    return SIMD_MULADD_F(
        y,
        VectorGlobals::f_coordImagScale_vec,
        VectorGlobals::f_coordImagOffset_vec
    );
}

FORCE_INLINE simd_full_t getRealPoints_vec(int width, scalar_full_t x) {
    using namespace VectorGlobals;

    const simd_full_t vals = getCenterReal_vec(
        SIMD_ADD_F(SIMD_SET1_F(x), _f_idx_vec)
    );

    return SIMD_BLEND_F(
        f_bailout_vec, vals,
        _validMasks[width - 1]
    );
}

#endif