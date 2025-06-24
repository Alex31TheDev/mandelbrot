#pragma once
#ifdef USE_VECTORS
#include "CommonDefs.h"

#include <immintrin.h>
#ifdef USE_SLEEF
#include "../util/WarningUtil.h"

PUSH_DISABLE_WARNINGS
#include <sleef.h>
POP_DISABLE_WARNINGS
#endif

#include "../scalar/ScalarTypes.h"

#include "../util/MacroUtil.h"
#include "../util/InlineUtil.h"

#define SIMD_SYM_F(a) _CONCAT2(a, SIMD_FULL_ARCH_WIDTH)

#if AVX512
#define SIMD_FULL_ARCH_WIDTH 512
#elif AVX2
#define  SIMD_FULL_ARCH_WIDTH 256
#elif SSE
#define SIMD_FULL_ARCH_WIDTH 128

#undef SIMD_SYM_F
#define SIMD_SYM_F(a) _EVAL(a)
#else
#error "No supported SIMD instruction set detected (requires SSE4.2, AVX2 or AVX512)."
#endif

#ifdef USE_SLEEF

#if AVX512
#define _SLEEF_FULL_ARCH_NAME avx512
#elif AVX2
#define _SLEEF_FULL_ARCH_NAME avx2
#elif SSE
#define _SLEEF_FULL_ARCH_NAME sse2
#endif

#endif

#if defined(USE_FLOATS)
#define SIMD_HALF_ARCH_WIDTH SIMD_FULL_ARCH_WIDTH
#define SIMD_SYM_H SIMD_SYM_F

#define simd_full_t _CONCAT2(__m, SIMD_FULL_ARCH_WIDTH)

#define _SLEEF_HALF_ARCH_NAME _SLEEF_FULL_ARCH_NAME
#elif defined(USE_DOUBLES)
#define SIMD_SYM_H(a) _EVAL(a)

#if AVX512
#define SIMD_HALF_ARCH_WIDTH 256

#undef SIMD_SYM_H
#define SIMD_SYM_H(a) _CONCAT2(a, SIMD_HALF_ARCH_WIDTH)

#define _SLEEF_HALF_ARCH_NAME avx2
#elif AVX2
#define SIMD_HALF_ARCH_WIDTH 128
#define _SLEEF_HALF_ARCH_NAME avx2128
#elif SSE
#define SIMD_HALF_ARCH_WIDTH 128
#define _SLEEF_HALF_ARCH_NAME sse2
#endif

#define simd_full_t _CONCAT3(__m, SIMD_FULL_ARCH_WIDTH, d)
#else
#error "Must define either USE_FLOATS or USE_DOUBLES to select precision."
#endif
#define simd_half_t _CONCAT2(__m, SIMD_HALF_ARCH_WIDTH)

#define HALF_SUFFIX ps
#define _HALF_SIZE 32

#if defined(USE_FLOATS)
#define FULL_SUFFIX HALF_SUFFIX
#define _FULL_SIZE _HALF_SIZE
#elif defined(USE_DOUBLES)
#define FULL_SUFFIX pd
#define _FULL_SIZE 64
#endif

#ifdef USE_SLEEF
#define _SLEEF_HALF_SUFFIX f

#if defined(USE_FLOATS)
#define _SLEEF_FULL_SUFFIX _SLEEF_HALF_SUFFIX
#elif defined(USE_DOUBLES)
#define _SLEEF_FULL_SUFFIX d
#endif

#endif

#if defined(USE_FLOATS)

#if AVX512
#define SIMD_FULL_WIDTH 16
#elif AVX2
#define SIMD_FULL_WIDTH 8
#elif SSE
#define SIMD_FULL_WIDTH 4
#endif

#elif defined(USE_DOUBLES)

#if AVX512
#define SIMD_FULL_WIDTH 8
#elif AVX2
#define SIMD_FULL_WIDTH 4
#elif SSE
#define SIMD_FULL_WIDTH 2
#endif

#endif
#define SIMD_HALF_WIDTH SIMD_FULL_WIDTH

constexpr int SIMD_FULL_ALIGNMENT = SIMD_FULL_ARCH_WIDTH / 8;
constexpr int SIMD_HALF_ALIGNMENT = SIMD_HALF_ARCH_WIDTH / 8;

struct simd_full_2_t {
    simd_full_t x, y;
};
struct simd_half_2_t {
    simd_half_t x, y;
};

#define simd_full_int_t _CONCAT3(__m, SIMD_FULL_ARCH_WIDTH, i)
#define simd_half_int_t _CONCAT3(__m, SIMD_HALF_ARCH_WIDTH, i)

#if AVX512
#define simd_full_mask_t _CONCAT2(__mmask, SIMD_FULL_WIDTH)
#define simd_half_mask_t _CONCAT2(__mmask, SIMD_HALF_WIDTH)

#define simd_full_int_mask_t simd_full_mask_t
#define simd_half_int_mask_t simd_half_mask_t
#else
#define simd_full_mask_t simd_full_t
#define simd_half_mask_t simd_half_t

#define simd_full_int_mask_t simd_full_int_t
#define simd_half_int_mask_t simd_half_int_t
#endif

#define _SIMD_FUNC_F_IMPL(name, suffix) \
    _CONCAT3(SIMD_SYM_F(_mm), _ ## name ## _, suffix)
#define _SIMD_FUNC_H_IMPL(name, suffix) \
    _CONCAT3(SIMD_SYM_H(_mm), _ ## name ## _, suffix)

#define SIMD_FUNC_F(name, suffix, ...) \
    _SIMD_FUNC_F_IMPL(name, suffix)(__VA_ARGS__)
#define SIMD_FUNC_H(name, suffix, ...) \
    _SIMD_FUNC_H_IMPL(name, suffix)(__VA_ARGS__)

#define SIMD_FUNC_DEC_F(name, ...) SIMD_FUNC_F(name, FULL_SUFFIX, __VA_ARGS__)
#define SIMD_FUNC_DEC_H(name, ...) SIMD_FUNC_H(name, HALF_SUFFIX, __VA_ARGS__)

#define SIMD_FUNC_INT_F(name, size, ...) \
    SIMD_FUNC_F(name, _CONCAT2(epi, size), __VA_ARGS__)
#define SIMD_FUNC_INT_H(name, size, ...) \
    SIMD_FUNC_H(name, _CONCAT2(epi, size), __VA_ARGS__)

#define SIMD_FUNC_DEC_MASK_F(name, ...) \
    SIMD_FUNC_F(name, _CONCAT2(FULL_SUFFIX, _mask), __VA_ARGS__)
#define SIMD_FUNC_DEC_MASK_H(name, ...) \
    SIMD_FUNC_H(name, _CONCAT2(HALF_SUFFIX, _mask), __VA_ARGS__)

#define SIMD_FUNC_INT_MASK_F(name, size, ...) \
    SIMD_FUNC_F(name, epi ## size ## _mask, __VA_ARGS__)
#define SIMD_FUNC_INT_MASK_H(name, size, ...) \
    SIMD_FUNC_H(name, epi ## size ## _mask, __VA_ARGS__)

#ifdef USE_SLEEF

#define SLEEF_FUNC_DEC_F(name, prec, ...) \
    _CONCAT5(Sleef_ ## name, _SLEEF_FULL_SUFFIX, \
    SIMD_FULL_WIDTH, _u ## prec, _SLEEF_FULL_ARCH_NAME)(__VA_ARGS__)
#define SLEEF_FUNC_DEC_H(name, prec, ...) \
    _CONCAT5(Sleef_ ## name, _SLEEF_HALF_SUFFIX, \
    SIMD_HALF_WIDTH, _u ## prec, _SLEEF_HALF_ARCH_NAME)(__VA_ARGS__)

#endif

#define SIMD_LOAD_F(ptr) \
    SIMD_FUNC_DEC_F(loadu, reinterpret_cast<const scalar_full_t *>(ptr))
#define SIMD_LOAD_H(ptr) \
    SIMD_FUNC_DEC_H(loadu, reinterpret_cast<const scalar_half_t *>(ptr))

#define SIMD_STORE_F(ptr, vec) \
    SIMD_FUNC_DEC_F(store, reinterpret_cast<scalar_full_t *>(ptr), vec)
#define SIMD_STORE_H(ptr, vec) \
    SIMD_FUNC_DEC_H(store, reinterpret_cast<scalar_half_t *>(ptr), vec)

#define SIMD_SET_F(x) SIMD_FUNC_DEC_F(set1, CAST_F(x))
#define SIMD_SET_H(x) SIMD_FUNC_DEC_H(set1, CAST_H(x))

/*
#if _FULL_SIZE == 32
#define SIMD_..._INT_F SIMD_..._INT32_F
#elif _FULL_SIZE == 64
#define SIMD_..._INT_F SIMD_..._INT64_F
#endif

#if _HALF_SIZE == 32
#define SIMD_..._INT_H SIMD_..._INT32_H
#elif _HALF_SIZE == 64
#define SIMD_..._INT_H SIMD_..._INT64_H
#endif
*/

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

#define _INT32_SIZE_SYM_F 32
#define _INT32_SIZE_SYM_H 32

#define SIMD_LOAD_INT_F(ptr)                                   \
    _CONCAT3(SIMD_SYM_F(_mm), _loadu_si, SIMD_FULL_ARCH_WIDTH) \
    (reinterpret_cast<const simd_full_int_t *>(ptr))
#define SIMD_LOAD_INT_H(ptr)                                   \
    _CONCAT3(SIMD_SYM_H(_mm), _loadu_si, SIMD_HALF_ARCH_WIDTH) \
    (reinterpret_cast<const simd_half_int_t *>(ptr))

#define SIMD_STORE_INT_F(ptr, vec)                             \
    _CONCAT3(SIMD_SYM_F(_mm), _store_si, SIMD_FULL_ARCH_WIDTH) \
    (reinterpret_cast<simd_full_int_t *>(ptr), vec)
#define SIMD_STORE_INT_H(ptr, vec)                             \
    _CONCAT3(SIMD_SYM_H(_mm), _store_si, SIMD_HALF_ARCH_WIDTH) \
    (reinterpret_cast<simd_half_int_t *>(ptr), vec)

#define SIMD_SET_INT32_F(x) \
    SIMD_FUNC_INT_F(set1, _INT32_SIZE_SYM_F, CAST_INT_S(x, 32))
#define SIMD_SET_INT32_H(x) \
    SIMD_FUNC_INT_H(set1, _INT32_SIZE_SYM_H, CAST_INT_S(x, 32))

#define SIMD_SET_INT64_F(x) \
    SIMD_FUNC_INT_F(set1, _INT64_SIZE_SYM_F, CAST_INT_S(x, 64))
#define SIMD_SET_INT64_H(x) \
    SIMD_FUNC_INT_H(set1, _INT64_SIZE_SYM_H, CAST_INT_S(x, 64))

#if _FULL_SIZE == 32
#define SIMD_SET_INT_F SIMD_SET_INT32_F
#elif _FULL_SIZE == 64
#define SIMD_SET_INT_F SIMD_SET_INT64_F
#endif

#if _HALF_SIZE == 32
#define SIMD_SET_INT_H SIMD_SET_INT32_H
#elif _HALF_SIZE == 64
#define SIMD_SET_INT_H SIMD_SET_INT64_H
#endif

#if defined(USE_FLOATS)
#define SIMD_FULL_TO_HALF_CONV(x) x
#elif defined(USE_DOUBLES)

#if AVX512

FORCE_INLINE __m256 SIMD_FULL_TO_HALF_CONV(const __m512d &x) {
    const __m256d f_low = _mm512_castpd512_pd256(x);
    const __m256d f_high = _mm512_extractf64x4_pd(x, 1);

    const __m128 h_low = _mm256_cvtpd_ps(f_low);
    const __m128 h_high = _mm256_cvtpd_ps(f_high);

    return _mm256_set_m128(h_high, h_low);
}

#else
#define SIMD_FULL_TO_HALF_CONV(x) \
    SIMD_FUNC_F(_CONCAT2(cvt, FULL_SUFFIX), HALF_SUFFIX, x)
#endif

#endif

#define SIMD_FULL_TO_INT_CONV(x) \
    SIMD_FUNC_INT_F(_CONCAT2(cvt, FULL_SUFFIX), _FULL_SIZE, x)
#define SIMD_HALF_TO_INT_CONV(x) \
    SIMD_FUNC_INT_H(_CONCAT2(cvt, HALF_SUFFIX), _HALF_SIZE, x)

#define SIMD_INT_TO_FULL_CONV(x) \
    SIMD_FUNC_INT_F(_CONCAT2(cvt, _FULL_SIZE), FULL_SUFFIX, x)
#define SIMD_INT_TO_HALF_CONV(x) \
    SIMD_FUNC_INT_H(_CONCAT2(cvt, _HALF_SIZE), HALF_SUFFIX, x)

#define SIMD_FULL_TO_INT_CAST(x) \
    SIMD_FUNC_F(_CONCAT2(cast, FULL_SUFFIX), \
    _CONCAT2(si, SIMD_FULL_ARCH_WIDTH), x)
#define SIMD_HALF_TO_INT_CAST(x) \
    SIMD_FUNC_H(_CONCAT2(cast, HALF_SUFFIX), \
    _CONCAT2(si, SIMD_HALF_ARCH_WIDTH), x)

#define SIMD_INT_TO_FULL_CAST(x) \
    SIMD_FUNC_DEC_F(_CONCAT2(castsi, SIMD_FULL_ARCH_WIDTH), x)
#define SIMD_INT_TO_HALF_CAST(x) \
    SIMD_FUNC_DEC_H(_CONCAT2(castsi, SIMD_HALF_ARCH_WIDTH), x)

constexpr int SIMD_ZERO_LANES_F = 0;
constexpr int SIMD_ZERO_LANES_H = 0;

constexpr int SIMD_ONES_LANES_F = (1 << SIMD_FULL_WIDTH) - 1;
constexpr int SIMD_ONES_LANES_H = (1 << SIMD_HALF_WIDTH) - 1;

#if AVX512
constexpr simd_full_mask_t SIMD_INIT_ZERO_MASK_F = SIMD_ZERO_LANES_F;
constexpr simd_full_mask_t SIMD_INIT_ZERO_MASK_H = SIMD_ZERO_LANES_H;

constexpr simd_full_mask_t SIMD_INIT_ONES_MASK_F = SIMD_ONES_LANES_F;
constexpr simd_full_mask_t SIMD_INIT_ONES_MASK_H = SIMD_ONES_LANES_H;
#else
#define SIMD_INIT_ZERO_MASK_F SIMD_SET_F(0)
#define SIMD_INIT_ZERO_MASK_H SIMD_SET_H(0)

#define SIMD_INIT_ONES_MASK_F SIMD_INT_TO_FULL_CAST(SIMD_SET_INT_F(-1))
#define SIMD_INIT_ONES_MASK_H SIMD_INT_TO_HALF_CAST(SIMD_SET_INT_H(-1))
#endif

#if AVX512
#define SIMD_FULL_MASK_TO_HALF(x) SIMD_AND_H(SIMD_SET_H(1), x)
#else
#define SIMD_FULL_MASK_TO_HALF SIMD_FULL_TO_HALF_CONV
#endif

#define SIMD_ADD_F(a, b) SIMD_FUNC_DEC_F(add, a, b)
#define SIMD_ADD_H(a, b) SIMD_FUNC_DEC_H(add, a, b)

#define SIMD_SUB_F(a, b) SIMD_FUNC_DEC_F(sub, a, b)
#define SIMD_SUB_H(a, b) SIMD_FUNC_DEC_H(sub, a, b)

#define SIMD_MUL_F(a, b) SIMD_FUNC_DEC_F(mul, a, b)
#define SIMD_MUL_H(a, b) SIMD_FUNC_DEC_H(mul, a, b)

#define SIMD_MULADD_F(a, b, c) SIMD_FUNC_DEC_F(fmadd, a, b, c)
#define SIMD_MULADD_H(a, b, c) SIMD_FUNC_DEC_H(fmadd, a, b, c)

#define SIMD_MULSUB_F(a, b, c) SIMD_FUNC_DEC_F(fmsub, a, b, c)
#define SIMD_MULSUB_H(a, b, c) SIMD_FUNC_DEC_H(fmsub, a, b, c)

#define SIMD_SUBMUL_F(a, b, c) SIMD_FUNC_DEC_F(fnmadd, a, b, c)
#define SIMD_SUBMUL_H(a, b, c) SIMD_FUNC_DEC_H(fnmadd, a, b, c)

#define SIMD_DIV_F(a, b) SIMD_FUNC_DEC_F(div, a, b)
#define SIMD_DIV_H(a, b) SIMD_FUNC_DEC_H(div, a, b)

#define SIMD_ADD_INT32_F(a, b) SIMD_FUNC_INT_F(add, 32, a, b)
#define SIMD_ADD_INT32_H(a, b) SIMD_FUNC_INT_H(add, 32, a, b)

#define SIMD_ADD_INT64_F(a, b) SIMD_FUNC_INT_F(add, 64, a, b)
#define SIMD_ADD_INT64_H(a, b) SIMD_FUNC_INT_H(add, 64, a, b)

#if _FULL_SIZE == 32
#define SIMD_ADD_INT_F SIMD_ADD_INT32_F
#elif _FULL_SIZE == 64
#define SIMD_ADD_INT_F SIMD_ADD_INT64_F
#endif

#if _HALF_SIZE == 32
#define SIMD_ADD_INT_H SIMD_ADD_INT32_H
#elif _HALF_SIZE == 64
#define SIMD_ADD_INT_H SIMD_ADD_INT64_H
#endif

#define SIMD_SUB_INT32_F(a, b) SIMD_FUNC_INT_F(sub, 32, a, b)
#define SIMD_SUB_INT32_H(a, b) SIMD_FUNC_INT_H(sub, 32, a, b)

#define SIMD_SUB_INT64_F(a, b) SIMD_FUNC_INT_F(sub, 64, a, b)
#define SIMD_SUB_INT64_H(a, b) SIMD_FUNC_INT_H(sub, 64, a, b)

#if _FULL_SIZE == 32
#define SIMD_SUB_INT_F SIMD_SUB_INT32_F
#elif _FULL_SIZE == 64
#define SIMD_SUB_INT_F SIMD_SUB_INT64_F
#endif

#if _HALF_SIZE == 32
#define SIMD_SUB_INT_H SIMD_SUB_INT32_H
#elif _HALF_SIZE == 64
#define SIMD_SUB_INT_H SIMD_SUB_INT64_H
#endif

#define SIMD_MUL_INT32_F(a, b) SIMD_FUNC_INT_F(mul, 32, a, b)
#define SIMD_MUL_INT32_H(a, b) SIMD_FUNC_INT_H(mul, 32, a, b)

#define SIMD_MUL_INT64_F(a, b) SIMD_FUNC_INT_F(mul, 64, a, b)
#define SIMD_MUL_INT64_H(a, b) SIMD_FUNC_INT_H(mul, 64, a, b)

#if _FULL_SIZE == 32
#define SIMD_MUL_INT_F SIMD_MUL_INT32_F
#elif _FULL_SIZE == 64
#define SIMD_MUL_INT_F SIMD_MUL_INT64_F
#endif

#if _HALF_SIZE == 32
#define SIMD_MUL_INT_H SIMD_MUL_INT32_H
#elif _HALF_SIZE == 64
#define SIMD_MUL_INT_H SIMD_MUL_INT64_H
#endif

#if AVX512
#define SIMD_MASK_F(x) static_cast<simd_full_mask_t>(x)
#define SIMD_MASK_H(x) static_cast<simd_half_mask_t>(x)
#else
#define SIMD_MASK_F(x) SIMD_FUNC_DEC_F(movemask, x)
#define SIMD_MASK_H(x) SIMD_FUNC_DEC_H(movemask, x)
#endif

#if AVX512
#define SIMD_AND_F(a, b) SIMD_FUNC_DEC_F(maskz_and, b, a)
#define SIMD_AND_H(a, b) SIMD_FUNC_DEC_H(maskz_and, b, a)
#else
#define SIMD_AND_F(a, b) SIMD_FUNC_DEC_F(and, a, b)
#define SIMD_AND_H(a, b) SIMD_FUNC_DEC_H(and, a, b)
#endif

#define SIMD_AND_INT_F(a, b) \
    SIMD_FUNC_F(and, _CONCAT2(si, SIMD_FULL_ARCH_WIDTH), a, b)
#define SIMD_AND_INT_H(a, b) \
    SIMD_FUNC_H(and, _CONCAT2(si, SIMD_HALF_ARCH_WIDTH), a, b)

#if AVX512
#define SIMD_AND_MASK_F(a, b) (a) & (b)
#define SIMD_AND_MASK_H SIMD_AND_MASK_F
#else
#define SIMD_AND_MASK_F SIMD_AND_F
#define SIMD_AND_MASK_H SIMD_AND_H
#endif

#if AVX512
#define SIMD_OR_F(a, b) SIMD_FUNC_DEC_F(maskz_or, b, a)
#define SIMD_OR_H(a, b) SIMD_FUNC_DEC_H(maskz_or, b, a)
#else
#define SIMD_OR_F(a, b) SIMD_FUNC_DEC_F(or, a, b)
#define SIMD_OR_H(a, b) SIMD_FUNC_DEC_H(or, a, b)
#endif

#if AVX512
#define SIMD_OR_MASK_F(a, b) (a) | (b)
#define SIMD_OR_MASK_H SIMD_OR_MASK_F
#else
#define SIMD_OR_MASK_F SIMD_OR_F
#define SIMD_OR_MASK_H SIMD_OR_H
#endif

#if AVX512
#define SIMD_XOR_F(a, b) SIMD_FUNC_DEC_F(maskz_xor, b, a)
#define SIMD_XOR_H(a, b) SIMD_FUNC_DEC_H(maskz_xor, b, a)
#else
#define SIMD_XOR_F(a, b) SIMD_FUNC_DEC_F(xor, a, b)
#define SIMD_XOR_H(a, b) SIMD_FUNC_DEC_H(xor, a, b)
#endif

#if AVX512
#define SIMD_XOR_MASK_F(a, b) (a) ^ (b)
#define SIMD_XOR_MASK_H SIMD_XOR_MASK_F
#else
#define SIMD_XOR_MASK_F SIMD_XOR_F
#define SIMD_XOR_MASK_H SIMD_XOR_H
#endif

#if AVX512
#define SIMD_NOT_MASK_F(x) ~(x)
#define SIMD_NOT_MASK_H SIMD_NOT_MASK_F
#else
#define SIMD_NOT_MASK_F(x) SIMD_XOR_F(x, SIMD_INIT_ONES_MASK_F)
#define SIMD_NOT_MASK_H(x) SIMD_XOR_H(x, SIMD_INIT_ONES_MASK_H)
#endif

#if AVX512
#define SIMD_ANDNOT_F(a, b) SIMD_FUNC_DEC_F(maskz_andnot, b, a)
#define SIMD_ANDNOT_H(a, b) SIMD_FUNC_DEC_H(maskz_andnot, b, a)
#else
#define SIMD_ANDNOT_F(a, b) SIMD_FUNC_DEC_F(andnot, a, b)
#define SIMD_ANDNOT_H(a, b) SIMD_FUNC_DEC_H(andnot, a, b)
#endif

#if AVX512
#define SIMD_ANDNOT_MASK_F(a, b) (a) & ~(b)
#define SIMD_ANDNOT_MASK_H SIMD_ANDNOT_MASK_F
#else
#define SIMD_ANDNOT_MASK_F SIMD_ANDNOT_F
#define SIMD_ANDNOT_MASK_H SIMD_ANDNOT_H
#endif

#if AVX512
#define SIMD_BLEND_F(a, b, x) SIMD_FUNC_DEC_F(mask_blend, x, a, b)
#define SIMD_BLEND_H(a, b, x) SIMD_FUNC_DEC_H(mask_blend, x, a, b)
#else
#define SIMD_BLEND_F(a, b, x) SIMD_FUNC_DEC_F(blendv, a, b, x)
#define SIMD_BLEND_H(a, b, x) SIMD_FUNC_DEC_H(blendv, a, b, x)
#endif

#if AVX512
#define SIMD_BLEND_INT8_F(a, b, x) SIMD_FUNC_INT_F(mask_blend, 8, x, a, b)
#define SIMD_BLEND_INT8_H(a, b, x) SIMD_FUNC_INT_H(mask_blend, 8, x, a, b)
#else
#define SIMD_BLEND_INT8_F(a, b, x) SIMD_FUNC_INT_F(blendv, 8, a, b, x)
#define SIMD_BLEND_INT8_H(a, b, x) SIMD_FUNC_INT_H(blendv, 8, a, b, x)
#endif

#if AVX512
#define SIMD_ADD_MASK_F(a, b, mask) SIMD_FUNC_DEC_F(mask_add, a, mask, a, b)
#define SIMD_ADD_MASK_H(a, b, mask) SIMD_FUNC_DEC_H(mask_add, a, mask, a, b)
#else
#define SIMD_ADD_MASK_F(a, b, mask) SIMD_ADD_F(a, SIMD_AND_F(mask, b))
#define SIMD_ADD_MASK_H(a, b, mask) SIMD_ADD_H(a, SIMD_AND_H(mask, b))
#endif

#if AVX512

#define SIMD_ADD_INT32_MASK_F(a, b, mask) \
    SIMD_FUNC_INT_F(mask_add, 32, a, mask, a, b)
#define SIMD_ADD_INT32_MASK_H(a, b, mask) \
    SIMD_FUNC_INT_H(mask_add, 32, a, mask, a, b)

#define SIMD_ADD_INT64_MASK_F(a, b, mask) \
    SIMD_FUNC_INT_F(mask_add, 32, a, mask, a, b)
#define SIMD_ADD_INT64_MASK_H(a, b, mask) \
    SIMD_FUNC_INT_H(mask_add, 32, a, mask, a, b)

#if _FULL_SIZE == 32
#define SIMD_ADD_INT_MASK_F SIMD_ADD_INT64_MASK_H
#elif _FULL_SIZE == 64
#define SIMD_ADD_INT_MASK_F SIMD_ADD_INT64_MASK_H
#endif

#if _HALF_SIZE == 32
#define SIMD_ADD_INT_MASK_H SIMD_ADD_INT32_MASK_H
#elif _HALF_SIZE == 64
#define SIMD_ADD_INT_MASK_H SIMD_ADD_INT32_MASK_H
#endif

#else

#define SIMD_ADD_INT_MASK_F(a, b, mask) \
    SIMD_ADD_INT_F(a, SIMD_AND_INT_H(mask, b))
#define SIMD_ADD_INT_MASK_H(a, b, mask) \
    SIMD_ADD_INT_H(a, SIMD_AND_INT_H(mask, b))

#endif

#if AVX512
#define SIMD_SUB_MASK_F(a, b, mask) SIMD_FUNC_DEC_F(mask_sub, a, mask, a, b)
#define SIMD_SUB_MASK_H(a, b, mask) SIMD_FUNC_DEC_H(mask_sub, a, mask, a, b)
#else
#define SIMD_SUB_MASK_F(a, b, mask) SIMD_SUB_F(a, SIMD_AND_F(mask, b))
#define SIMD_SUB_MASK_H(a, b, mask) SIMD_SUB_H(a, SIMD_AND_H(mask, b))
#endif

#if AVX512
#define SIMD_MUL_MASK_F(a, b, mask) SIMD_FUNC_DEC_F(mask_mul, a, mask, a, b)
#define SIMD_MUL_MASK_H(a, b, mask) SIMD_FUNC_DEC_H(mask_mul, a, mask, a, b)
#else
#define SIMD_MUL_MASK_F(a, b, mask) SIMD_BLEND_F(a, SIMD_MUL_F(a, b), mask)
#define SIMD_MUL_MASK_H(a, b, mask) SIMD_BLEND_H(a, SIMD_MUL_H(a, b), mask)
#endif

#define SIMD_MIN_F(a, b) SIMD_FUNC_DEC_F(min, a, b)
#define SIMD_MIN_H(a, b) SIMD_FUNC_DEC_H(min, a, b)

#define SIMD_MAX_F(a, b) SIMD_FUNC_DEC_F(max, a, b)
#define SIMD_MAX_H(a, b) SIMD_FUNC_DEC_H(max, a, b)

#define SIMD_NEG_F(x) SIMD_XOR_F(x, SIMD_SET_F(-0.0))
#define SIMD_NEG_H(x) SIMD_XOR_H(x, SIMD_SET_H(-0.0))

#define SIMD_ABS_F(x) SIMD_ANDNOT_F(SIMD_SET_F(-0.0), x)
#define SIMD_ABS_H(x) SIMD_ANDNOT_H(SIMD_SET_H(-0.0), x)

#if AVX512

FORCE_INLINE __m512 _floor_ps_impl(const __m512 &x) {
    __m512i tmp = _mm512_cvttps_epi32(x);
    const __m512 floored = _mm512_cvtepi32_ps(tmp);
    const __mmask16 needsAdj = _mm512_cmp_ps_mask(floored, x, _CMP_GT_OQ);
    tmp = _mm512_mask_sub_epi32(tmp, needsAdj, tmp, _mm512_set1_epi32(1));
    return _mm512_cvtepi32_ps(tmp);
}
FORCE_INLINE __m512d _floor_pd_impl(const __m512d &x) {
    __m512i tmp = _mm512_cvttpd_epi64(x);
    const __m512d floored = _mm512_cvtepi64_pd(tmp);
    const __mmask8 needsAdj = _mm512_cmp_pd_mask(floored, x, _CMP_GT_OQ);
    tmp = _mm512_mask_sub_epi64(tmp, needsAdj, tmp, _mm512_set1_epi64(1));
    return _mm512_cvtepi64_pd(tmp);
}

#if SIMD_FULL_ARCH_WIDTH == 512
#define SIMD_FLOOR_F _CONCAT3(_floor_, FULL_SUFFIX, _impl)
#else
#define SIMD_FLOOR_F(x) SIMD_FUNC_DEC_F(floor, x)
#endif

#if SIMD_HALF_ARCH_WIDTH == 512
#define SIMD_FLOOR_H _CONCAT3(_floor_, HALF_SUFFIX, _impl)
#else
#define SIMD_FLOOR_H(x) SIMD_FUNC_DEC_H(floor, x)
#endif

#else
#define SIMD_FLOOR_F(x) SIMD_FUNC_DEC_F(floor, x)
#define SIMD_FLOOR_H(x) SIMD_FUNC_DEC_H(floor, x)
#endif

#if AVX512
#define SIMD_CMP_F(a, b, x) SIMD_FUNC_DEC_MASK_F(cmp, a, b, x)
#define SIMD_CMP_H(a, b, x) SIMD_FUNC_DEC_MASK_H(cmp, a, b, x)
#elif AVX2
#define SIMD_CMP_F(a, b, x) SIMD_FUNC_DEC_F(cmp, a, b, x)
#define SIMD_CMP_H(a, b, x) SIMD_FUNC_DEC_H(cmp, a, b, x)
#endif

#if AVX512 || AVX2

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

#elif SSE

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

#endif

#if AVX512
#define _INT_CMP_FUNC_F(type, size, ...) \
    SIMD_FUNC_INT_MASK_F(cmp ## type, size, __VA_ARGS__)
#define _INT_CMP_FUNC_H(type, size, ...) \
    SIMD_FUNC_INT_MASK_H(cmp ## type, size, __VA_ARGS__)
#else
#define _INT_CMP_FUNC_F(type, size, ...) \
    SIMD_FUNC_INT_F(cmp ## type, size, __VA_ARGS__)
#define _INT_CMP_FUNC_H(type, size, ...) \
     SIMD_FUNC_INT_H(cmp ## type, size, __VA_ARGS__)
#endif

#define SIMD_CMP_INT32_EQ_F(a, b) _INT_CMP_FUNC_F(eq, 32, a, b)
#define SIMD_CMP_INT32_EQ_H(a, b) _INT_CMP_FUNC_H(eq, 32, a, b)

#define SIMD_CMP_INT64_EQ_F(a, b) _INT_CMP_FUNC_F(eq, 64, a, b)
#define SIMD_CMP_INT64_EQ_H(a, b) _INT_CMP_FUNC_H(eq, 64, a, b)

#define SIMD_CMP_INT32_NEQ_F(a, b) _INT_CMP_FUNC_F(neq, 32, a, b)
#define SIMD_CMP_INT32_NEQ_H(a, b) _INT_CMP_FUNC_H(neq, 32, a, b)

#define SIMD_CMP_INT64_NEQ_F(a, b) _INT_CMP_FUNC_F(neq, 64, a, b)
#define SIMD_CMP_INT64_NEQ_H(a, b) _INT_CMP_FUNC_H(neq, 64, a, b)

#define SIMD_CMP_INT32_LT_F(a, b) _INT_CMP_FUNC_F(lt, 32, a, b)
#define SIMD_CMP_INT32_LT_H(a, b) _INT_CMP_FUNC_H(lt, 32, a, b)

#define SIMD_CMP_INT64_LT_F(a, b) _INT_CMP_FUNC_F(lt, 64, a, b)
#define SIMD_CMP_INT64_LT_H(a, b) _INT_CMP_FUNC_H(lt, 64, a, b)

#define SIMD_CMP_INT32_GT_F(a, b) _INT_CMP_FUNC_F(gt, 32, a, b)
#define SIMD_CMP_INT32_GT_H(a, b) _INT_CMP_FUNC_H(gt, 32, a, b)

#define SIMD_CMP_INT64_GT_F(a, b) _INT_CMP_FUNC_F(gt, 64, a, b)
#define SIMD_CMP_INT64_GT_H(a, b) _INT_CMP_FUNC_H(gt, 64, a, b)

#define SIMD_CMP_INT32_LE_F(a, b) _INT_CMP_FUNC_F(le, 32, a, b)
#define SIMD_CMP_INT32_LE_H(a, b) _INT_CMP_FUNC_H(le, 32, a, b)

#define SIMD_CMP_INT64_LE_F(a, b) _INT_CMP_FUNC_F(le, 64, a, b)
#define SIMD_CMP_INT64_LE_H(a, b) _INT_CMP_FUNC_H(le, 64, a, b)

#define SIMD_CMP_INT32_GE_F(a, b) _INT_CMP_FUNC_F(ge, 32, a, b)
#define SIMD_CMP_INT32_GE_H(a, b) _INT_CMP_FUNC_H(ge, 32, a, b)

#define SIMD_CMP_INT64_GE_F(a, b) _INT_CMP_FUNC_F(ge, 64, a, b)
#define SIMD_CMP_INT64_GE_H(a, b) _INT_CMP_FUNC_H(ge, 64, a, b)

#if _FULL_SIZE == 32
#define SIMD_CMP_INT_EQ_F SIMD_CMP_INT32_EQ_F
#define SIMD_CMP_INT_NEQ_F SIMD_CMP_INT32_NEQ_F
#define SIMD_CMP_INT_LT_F SIMD_CMP_INT32_LT_F
#define SIMD_CMP_INT_GT_F SIMD_CMP_INT32_GT_F
#define SIMD_CMP_INT_LE_F SIMD_CMP_INT32_LE_F
#define SIMD_CMP_INT_GE_F SIMD_CMP_INT32_GE_F

#elif _FULL_SIZE == 64
#define SIMD_CMP_INT_EQ_F SIMD_CMP_INT64_EQ_F
#define SIMD_CMP_INT_NEQ_F SIMD_CMP_INT64_NEQ_F
#define SIMD_CMP_INT_LT_F SIMD_CMP_INT64_LT_F
#define SIMD_CMP_INT_GT_F SIMD_CMP_INT64_GT_F
#define SIMD_CMP_INT_LE_F SIMD_CMP_INT64_LE_F
#define SIMD_CMP_INT_GE_F SIMD_CMP_INT64_GE_F
#endif

#if _HALF_SIZE == 32
#define SIMD_CMP_INT_EQ_H SIMD_CMP_INT32_EQ_H
#define SIMD_CMP_INT_NEQ_H SIMD_CMP_INT32_NEQ_H
#define SIMD_CMP_INT_LT_H SIMD_CMP_INT32_LT_H
#define SIMD_CMP_INT_GT_H SIMD_CMP_INT32_GT_H
#define SIMD_CMP_INT_LE_H SIMD_CMP_INT32_LE_H
#define SIMD_CMP_INT_GE_H SIMD_CMP_INT32_GE_H
#elif _HALF_SIZE == 64
#define SIMD_CMP_INT_EQ_H SIMD_CMP_INT64_EQ_H
#define SIMD_CMP_INT_NEQ_H SIMD_CMP_INT64_NEQ_H
#define SIMD_CMP_INT_LT_H SIMD_CMP_INT64_LT_H
#define SIMD_CMP_INT_GT_H SIMD_CMP_INT64_GT_H
#define SIMD_CMP_INT_LE_H SIMD_CMP_INT64_LE_H
#define SIMD_CMP_INT_GE_H SIMD_CMP_INT64_GE_H
#endif

#ifdef USE_SLEEF
#define SIMD_SQRT_F(x) SLEEF_FUNC_DEC_F(sqrt, 05, x)
#define SIMD_SQRT_H(x) SLEEF_FUNC_DEC_H(sqrt, 05, x)

#define SIMD_POW_F(a, b) SLEEF_FUNC_DEC_F(pow, 10, a, b)
#define SIMD_POW_H(a, b) SLEEF_FUNC_DEC_H(pow, 10, a, b)

#define SIMD_LOG_F(x) SLEEF_FUNC_DEC_F(log, 10, x)
#define SIMD_LOG_H(x) SLEEF_FUNC_DEC_H(log, 10, x)

#define SIMD_SIN_F(x) SLEEF_FUNC_DEC_F(sin, 35, x)
#define SIMD_SIN_H(x) SLEEF_FUNC_DEC_H(sin, 35, x)

#define SIMD_COS_F(x) SLEEF_FUNC_DEC_F(cos, 35, x)
#define SIMD_COS_H(x) SLEEF_FUNC_DEC_H(cos, 35, x)

#define SIMD_ATAN2_F(a, b) SLEEF_FUNC_DEC_F(atan2, 35, a, b)
#define SIMD_ATAN2_H(a, b) SLEEF_FUNC_DEC_H(atan2, 35, a, b)

#define _SIMD_SINCOS_F_IMPL(x) SLEEF_FUNC_DEC_F(sincos, 35, x)
#define _SIMD_SINCOS_H_IMPL(x) SLEEF_FUNC_DEC_H(sincos, 35, x)

FORCE_INLINE simd_full_2_t SIMD_SINCOS_F(const simd_full_t &x) {
    auto val = _SIMD_SINCOS_F_IMPL(x);
    return reinterpret_cast<simd_full_2_t &>(val);
}
FORCE_INLINE simd_half_2_t SIMD_SINCOS_H(const simd_half_t &x) {
    auto val = _SIMD_SINCOS_H_IMPL(x);
    return reinterpret_cast<simd_half_2_t &>(val);
}

#else
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

#define _SIMD_SINCOS_F_IMPL(ptr, x) SIMD_FUNC_DEC_F(sincos, ptr, x)
#define _SIMD_SINCOS_H_IMPL(ptr, x) SIMD_FUNC_DEC_H(sincos, ptr, x)

FORCE_INLINE simd_full_2_t SIMD_SINCOS_F(const simd_full_t &x) {
    simd_full_2_t out{};
    out.x = _SIMD_SINCOS_F_IMPL(&out.y, x);
    return out;
}
FORCE_INLINE simd_half_2_t SIMD_SINCOS_H(const simd_half_t &x) {
    simd_half_2_t out{};
    out.x = _SIMD_SINCOS_H_IMPL(&out.y, x);
    return out;
}

#endif

#if defined(USE_FLOATS)
#define _GATHER_SCALE_F 4
#define _GATHER_SCALE_H _GATHER_SCALE_F
#elif defined(USE_DOUBLES)
#define _GATHER_SCALE_F 8
#define _GATHER_SCALE_H 4
#endif

#if SIMD_HALF_ARCH_WIDTH == 512

#define SIMD_GATHER_INT32_H(ptr, idx) \
    SIMD_FUNC_DEC_H(i32gather, idx, ptr, _GATHER_SCALE_H)
#define SIMD_GATHER_INT64_H(ptr, idx) \
    SIMD_FUNC_DEC_H(i64gather, idx, ptr, _GATHER_SCALE_H)

#else

#define SIMD_GATHER_INT32_H(ptr, idx) \
    SIMD_FUNC_DEC_H(i32gather, ptr, idx, _GATHER_SCALE_H)
#define SIMD_GATHER_INT64_H(ptr, idx) \
    SIMD_FUNC_DEC_H(i64gather, ptr, idx, _GATHER_SCALE_H)

#endif

#if _HALF_SIZE == 32
#define SIMD_GATHER_H SIMD_GATHER_INT32_H
#elif _HALF_SIZE == 64
#define SIMD_GATHER_H SIMD_GATHER_INT64_H
#endif

#define SIMD_PACK_USAT_INT16_H(a, x) SIMD_FUNC_INT_H(packus, 16, a, x)
#define SIMD_PACK_USAT_INT32_H(a, x) SIMD_FUNC_INT_H(packus, 32, a, x)
#define SIMD_SHUFFLE_INT8_H(a, x) SIMD_FUNC_INT_H(packus, 8, a, x)

#endif