#pragma once
#ifdef USE_VECTORS

#include <immintrin.h>

#include "../scalar/ScalarTypes.h"

#include "../util/MacroUtil.h"
#include "../util/InlineUtil.h"

#define SIMD_SYM_F(a) _CONCAT2(a, SIMD_FULL_ARCH_WIDTH)

#if defined(__AVX512__)
#define SIMD_FULL_ARCH_WIDTH 512
#elif defined(__AVX2__)
#define  SIMD_FULL_ARCH_WIDTH 256
#elif defined(__SSE2__)
#define SIMD_FULL_ARCH_WIDTH 128

#undef SIMD_SYM_F
#define SIMD_SYM_F(a) _EVAL(a)
#else
#define SIMD_FULL_ARCH_WIDTH 0
#endif

#ifdef USE_DOUBLES
#define SIMD_SYM_H(a) _EVAL(a)

#if defined(__AVX512__)
#define SIMD_HALF_ARCH_WIDTH 256

#undef SIMD_SYM_H
#define SIMD_SYM_H(a) _CONCAT2(a, SIMD_HALF_ARCH_WIDTH)
#elif defined(__AVX2__)
#define SIMD_HALF_ARCH_WIDTH 128
#elif defined(__SSE2__)
#define SIMD_HALF_ARCH_WIDTH 128
#else
#define SIMD_HALF_ARCH_WIDTH 0
#endif

#define simd_full_t _CONCAT3(__m, SIMD_FULL_ARCH_WIDTH, d)
#else
#define SIMD_HALF_ARCH_WIDTH SIMD_FULL_ARCH_WIDTH
#define SIMD_SYM_H SIMD_SYM_F

#define simd_full_t _CONCAT2(__m, SIMD_FULL_ARCH_WIDTH)
#endif
#define simd_half_t _CONCAT2(__m, SIMD_HALF_ARCH_WIDTH)

#define simd_full_int_t _CONCAT3(__m, SIMD_FULL_ARCH_WIDTH, i)
#define simd_half_int_t _CONCAT3(__m, SIMD_HALF_ARCH_WIDTH, i)

static constexpr int SIMD_FULL_ALIGNMENT = SIMD_FULL_ARCH_WIDTH / 8;
static constexpr int SIMD_HALF_ALIGNMENT = SIMD_HALF_ARCH_WIDTH / 8;

static constexpr int SIMD_FULL_WIDTH = SIMD_FULL_ARCH_WIDTH / (8 * sizeof(scalar_full_t));
static constexpr int SIMD_HALF_WIDTH = SIMD_HALF_ARCH_WIDTH / (8 * sizeof(scalar_half_t));

#if defined(__AVX512__)
#ifdef USE_DOUBLES
#define _SIMD_FULL_WIDTH 8
#define _SIMD_HALF_WIDTH 8
#else
#define _SIMD_FULL_WIDTH 16
#define _SIMD_HALF_WIDTH 16
#endif

#define simd_full_mask_t _CONCAT2(__mmask, _SIMD_FULL_WIDTH)
#define simd_half_mask_t _CONCAT2(__mmask, _SIMD_HALF_WIDTH)
#else
#define simd_full_mask_t simd_full_t
#define simd_half_mask_t simd_half_t
#endif

#ifdef USE_DOUBLES
#define FULL_SUFFIX pd
#else
#define FULL_SUFFIX ps
#endif
#define HALF_SUFFIX ps

#define _SIMD_FUNC_F_IMPL(name, suffix) _CONCAT3(SIMD_SYM_F(_mm), _ ## name ## _, suffix)
#define _SIMD_FUNC_H_IMPL(name, suffix) _CONCAT3(SIMD_SYM_H(_mm), _ ## name ## _, suffix)

#define SIMD_FUNC_F(name, suffix, ...) _SIMD_FUNC_F_IMPL(name, suffix)(__VA_ARGS__)
#define SIMD_FUNC_H(name, suffix, ...) _SIMD_FUNC_H_IMPL(name, suffix)(__VA_ARGS__)

#define SIMD_FUNC_DEC_F(name, ...) SIMD_FUNC_F(name, FULL_SUFFIX, __VA_ARGS__)
#define SIMD_FUNC_DEC_H(name, ...) SIMD_FUNC_H(name, HALF_SUFFIX, __VA_ARGS__)

#define SIMD_FUNC_INT_F(name, size, ...) SIMD_FUNC_F(name, _CONCAT2(epi, size), __VA_ARGS__)
#define SIMD_FUNC_INT_H(name, size, ...) SIMD_FUNC_H(name, _CONCAT2(epi, size), __VA_ARGS__)

#define SIMD_FUNC_MASK_F(name, ...) SIMD_FUNC_F(name, _CONCAT2(FULL_SUFFIX, _mask), __VA_ARGS__)
#define SIMD_FUNC_MASK_H(name, ...) SIMD_FUNC_H(name, _CONCAT2(HALF_SUFFIX, _mask), __VA_ARGS__)

#define SIMD_LOAD_F(ptr) SIMD_FUNC_DEC_F(loadu, reinterpret_cast<const scalar_full_t *>(ptr))
#define SIMD_LOAD_H(ptr) SIMD_FUNC_DEC_H(loadu, reinterpret_cast<const scalar_half_t *>(ptr))

#define SIMD_STORE_F(ptr, vec) SIMD_FUNC_DEC_F(store, reinterpret_cast<scalar_full_t *>(ptr), vec)
#define SIMD_STORE_H(ptr, vec) SIMD_FUNC_DEC_H(store, reinterpret_cast<scalar_half_t *>(ptr), vec)

#define SIMD_LOAD_INT_H(ptr)                                   \
    _CONCAT3(SIMD_SYM_H(_mm), _loadu_si, SIMD_HALF_ARCH_WIDTH) \
    (reinterpret_cast<const simd_half_int_t *>(ptr))
#define SIMD_STORE_INT_H(ptr, vec)                             \
    _CONCAT3(SIMD_SYM_H(_mm), _store_si, SIMD_HALF_ARCH_WIDTH) \
    (reinterpret_cast<simd_half_int_t *>(ptr), vec)

#define SIMD_SET_F(x) SIMD_FUNC_DEC_F(set1, CAST_F(x))
#define SIMD_SET_H(x) SIMD_FUNC_DEC_H(set1, CAST_H(x))

#if SIMD_FULL_ARCH_WIDTH == 512
#define _INT64_SIZE_SYM_F 64
#else
#define _INT64_SIZE_SYM_F 64x
#endif

#if SIMD_HALF_ARCH_WIDTH == 512
#define _INT64_SIZE_SYM_H 64
#else
#define _INT64_SIZE_SYM_H 64x
#endif

#define SIMD_SET_INT32_F(x) SIMD_FUNC_INT_F(set1, 32, CAST_INT_S(x, 32))
#define SIMD_SET_INT32_H(x) SIMD_FUNC_INT_H(set1, 32, CAST_INT_S(x, 32))

#define SIMD_SET_INT64_F(x) SIMD_FUNC_INT_F(set1, _INT64_SIZE_SYM_F, CAST_INT_S(x, 64))
#define SIMD_SET_INT64_H(x) SIMD_FUNC_INT_H(set1, _INT64_SIZE_SYM_H, CAST_INT_S(x, 64))

#ifdef USE_DOUBLES

#if defined(__AVX512__)
static FORCE_INLINE __m256 SIMD_FULL_TO_HALF(__m512d x) {
    __m256d f_low = _mm512_castpd512_pd256(x);
    __m256d f_high = _mm512_extractf64x4_pd(x, 1);

    __m128 h_low = _mm256_cvtpd_ps(f_low);
    __m128 h_high = _mm256_cvtpd_ps(f_high);

    return _mm256_set_m128(h_high, h_low);
}
#else
#define SIMD_FULL_TO_HALF(x) SIMD_FUNC_F(_CONCAT2(cvt, FULL_SUFFIX), HALF_SUFFIX, x)
#endif

#else
#define SIMD_FULL_TO_HALF(x) x
#endif

#define SIMD_HALF_TO_INT32(x) SIMD_FUNC_INT_H(_CONCAT2(cvt, HALF_SUFFIX), 32, x)
#define SIMD_FULL_INT_TO_FULL_CAST(x) SIMD_FUNC_DEC_F(_CONCAT2(castsi, SIMD_FULL_ARCH_WIDTH), x)

#if defined(__AVX512__)
#define SIMD_FULL_MASK_TO_HALF(x) SIMD_AND_H(SIMD_SET_H(1), x)
#else
#define SIMD_FULL_MASK_TO_HALF SIMD_FULL_TO_HALF
#endif

#define SIMD_MIN_H(a, b) SIMD_FUNC_DEC_H(min, a, b)
#define SIMD_MAX_H(a, b) SIMD_FUNC_DEC_H(max, a, b)

#define SIMD_ADD_F(a, b) SIMD_FUNC_DEC_F(add, a, b)
#define SIMD_ADD_H(a, b) SIMD_FUNC_DEC_H(add, a, b)

#if defined(__AVX512__)
#define SIMD_ADD_MASK_F(a, mask, b) SIMD_FUNC_DEC_F(mask_add, a, mask, a, b)
#else
#define SIMD_ADD_MASK_F(a, mask, b) SIMD_ADD_F(a, SIMD_AND_F(mask, b))
#endif

#define SIMD_SUB_F(a, b) SIMD_FUNC_DEC_F(sub, a, b)
#define SIMD_SUB_H(a, b) SIMD_FUNC_DEC_H(sub, a, b)

#define SIMD_MUL_F(a, b) SIMD_FUNC_DEC_F(mul, a, b)
#define SIMD_MUL_H(a, b) SIMD_FUNC_DEC_H(mul, a, b)

#define SIMD_DIV_F(a, b) SIMD_FUNC_DEC_F(div, a, b)
#define SIMD_DIV_H(a, b) SIMD_FUNC_DEC_H(div, a, b)

#define SIMD_SQRT_F(x) SIMD_FUNC_DEC_F(sqrt, x)
#define SIMD_SQRT_H(x) SIMD_FUNC_DEC_H(sqrt, x)

#define SIMD_POW_F(a, b) SIMD_FUNC_DEC_F(pow, a, b)
#define SIMD_POW_H(a, b) SIMD_FUNC_DEC_H(pow, a, b)

#define SIMD_LOG_F(x) SIMD_FUNC_DEC_F(log, x)
#define SIMD_LOG_H(x) SIMD_FUNC_DEC_H(log, x)

#define SIMD_SIN_F(x) SIMD_FUNC_DEC_F(sin, x)
#define SIMD_SIN_H(x) SIMD_FUNC_DEC_H(sin, x)

#define SIMD_COS_F(x) SIMD_FUNC_DEC_F(cos, x)
#define SIMD_COS_H(x) SIMD_FUNC_DEC_H(cos, x)

#define SIMD_ATAN2_F(a, b) SIMD_FUNC_DEC_F(atan2, a, b)
#define SIMD_ATAN2_H(a, b) SIMD_FUNC_DEC_H(atan2, a, b)

#if defined(__SSE2__)

#define SIMD_CMP_EQ_F(a, b) SIMD_FUNC_DEC_F(cmpeq, a, b)
#define SIMD_CMP_EQ_H(a, b) SIMD_FUNC_DEC_H(cmpeq, a, b)

#define SIMD_CMP_NEQ_F(a, b) SIMD_FUNC_DEC_F(cmpneq, a, b)
#define SIMD_CMP_NEQ_H(a, b) SIMD_FUNC_DEC_H(cmpneq, a, b)

#define SIMD_CMP_LT_F(a, b) SIMD_FUNC_DEC_F(cmplt, a, b)
#define SIMD_CMP_LT_H(a, b) SIMD_FUNC_DEC_H(cmplt, a, b)

#define SIMD_CMP_GT_F(a, b) SIMD_FUNC_DEC_F(cmpgt, a, b)
#define SIMD_CMP_GT_H(a, b) SIMD_FUNC_DEC_H(cmpgt, a, b)

#define SIMD_CMP_LE_F(a, b) SIMD_FUNC_DEC_F(cmple, a, b)
#define SIMD_CMP_LE_H(a, b) SIMD_FUNC_DEC_H(cmple, a, b)

#define SIMD_CMP_GE_F(a, b) SIMD_FUNC_DEC_F(cmpge, a, b)
#define SIMD_CMP_GE_H(a, b) SIMD_FUNC_DEC_H(cmpge, a, b)

#else

#if defined(__AVX512__)
#define SIMD_CMP_F(a, b, x) SIMD_FUNC_MASK_F(cmp, a, b, x)
#define SIMD_CMP_H(a, b, x) SIMD_FUNC_MASK_H(cmp, a, b, x)
#else
#define SIMD_CMP_F(a, b, x) SIMD_FUNC_DEC_F(cmp, a, b, x)
#define SIMD_CMP_H(a, b, x) SIMD_FUNC_DEC_H(cmp, a, b, x)
#endif

#define SIMD_CMP_EQ_F(a, b) SIMD_CMP_F(a, b, _CMP_EQ_OQ)
#define SIMD_CMP_EQ_H(a, b) SIMD_CMP_H(a, b, _CMP_EQ_OQ)

#define SIMD_CMP_NEQ_F(a, b) SIMD_CMP_F(a, b, _CMP_NEQ_OQ)
#define SIMD_CMP_NEQ_H(a, b) SIMD_CMP_H(a, b, _CMP_NEQ_OQ)

#define SIMD_CMP_LT_F(a, b) SIMD_CMP_F(a, b, _CMP_LT_OQ)
#define SIMD_CMP_LT_H(a, b) SIMD_CMP_H(a, b, _CMP_LT_OQ)

#define SIMD_CMP_GT_F(a, b) SIMD_CMP_F(a, b, _CMP_GT_OQ)
#define SIMD_CMP_GT_H(a, b) SIMD_CMP_H(a, b, _CMP_GT_OQ)

#define SIMD_CMP_LE_F(a, b) SIMD_CMP_F(a, b, _CMP_LE_OQ)
#define SIMD_CMP_LE_H(a, b) SIMD_CMP_H(a, b, _CMP_LE_OQ)

#define SIMD_CMP_GE_F(a, b) SIMD_CMP_F(a, b, _CMP_GE_OQ)
#define SIMD_CMP_GE_H(a, b) SIMD_CMP_H(a, b, _CMP_GE_OQ)

#endif

#if defined(__AVX512__)
#define SIMD_INIT_ZERO_MASK_F static_cast<simd_full_mask_t>(0)
#define SIMD_INIT_ONES_MASK_F ~SIMD_INIT_ZERO_MASK_F
#else
#define SIMD_INIT_ZERO_MASK_F SIMD_SET_F(0)
#define SIMD_INIT_ONES_MASK_F SIMD_FULL_INT_TO_FULL_CAST(SIMD_SET_INT32_F(-1))
#endif

#if defined(__AVX512__)
#define SIMD_MASK_F(x) static_cast<simd_full_mask_t>(x)
#else
#define SIMD_MASK_F(x) SIMD_FUNC_DEC_F(movemask, x)
#endif

#if defined(__AVX512__)
#define SIMD_AND_MASK_F(a, b) (a) & (b)
#else
#define SIMD_AND_MASK_F SIMD_AND_F
#endif

#if defined(__AVX512__)
#define SIMD_AND_F(a, b) SIMD_FUNC_DEC_F(maskz_mov, b, a)
#define SIMD_AND_H(a, b) SIMD_FUNC_DEC_H(maskz_mov, b, a)
#else
#define SIMD_AND_F(a, b) SIMD_FUNC_DEC_F(and, a, b)
#define SIMD_AND_H(a, b) SIMD_FUNC_DEC_H(and, a, b)
#endif

#define SIMD_ANDNOT_F(a, b) SIMD_FUNC_DEC_F(andnot, a, b)
#define SIMD_ABS_F(x) SIMD_ANDNOT_F(SIMD_SET_F(-0.0), x)

#if defined(__AVX512__)
#define SIMD_BLEND_F(a, b, x) SIMD_FUNC_DEC_F(mask_blend, x, a, b)
#else
#define SIMD_BLEND_F(a, b, x) SIMD_FUNC_DEC_F(blendv, a, b, x)
#endif

#define SIMD_PACK_USAT_INT16_H(a, x) SIMD_FUNC_INT_H(packus, 16, a, x)
#define SIMD_PACK_USAT_INT32_H(a, x) SIMD_FUNC_INT_H(packus, 32, a, x)
#define SIMD_SHUFFLE_INT8_H(a, x) SIMD_FUNC_INT_H(packus, 8, a, x)

#endif