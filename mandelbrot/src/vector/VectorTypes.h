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
#define SIMD_FULL_ARCH_WIDTH 256
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

#define SIMD_HALF_SIZE 32
#define SIMD_HALF_PACK_SUFFIX ps
#define SIMD_HALF_LOW_SUFFIX ss

#if defined(USE_FLOATS)

#define SIMD_FULL_SIZE SIMD_HALF_SIZE
#define SIMD_FULL_PACK_SUFFIX SIMD_HALF_PACK_SUFFIX
#define SIMD_FULL_LOW_SUFFIX SIMD_HALF_LOW_SUFFIX

#elif defined(USE_DOUBLES)

#define SIMD_FULL_SIZE 64
#define SIMD_FULL_PACK_SUFFIX pd
#define SIMD_FULL_LOW_SUFFIX sd

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

#define _SIMD_IMPL_SUFFIX_F \
    _CONCAT3(SIMD_FULL_PACK_SUFFIX, SIMD_FULL_ARCH_WIDTH, _impl)
#define _SIMD_IMPL_SUFFIX_H \
    _CONCAT3(SIMD_HALF_PACK_SUFFIX, SIMD_HALF_ARCH_WIDTH, _impl)

struct simd_full_2_t {
    simd_full_t x, y;
};
struct simd_half_2_t {
    simd_half_t x, y;
};

#define SIMD_FULL_INT_SUFFIX _CONCAT2(si, SIMD_FULL_ARCH_WIDTH)
#define SIMD_HALF_INT_SUFFIX _CONCAT2(si, SIMD_HALF_ARCH_WIDTH)

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

#if SIMD_FULL_ARCH_WIDTH == 256 && \
    SIMD_HALF_ARCH_WIDTH == 128
#define AVX_ZEROUPPER _mm256_zeroupper();
#else
#define AVX_ZEROUPPER
#endif

#endif

#define _SIMD_FUNC_F_IMPL(name, suffix) \
    _CONCAT3(SIMD_SYM_F(_mm), _ ## name ## _, suffix)
#define _SIMD_FUNC_H_IMPL(name, suffix) \
    _CONCAT3(SIMD_SYM_H(_mm), _ ## name ## _, suffix)

#define SIMD_FUNC_F(name, suffix, ...) \
    _SIMD_FUNC_F_IMPL(name, suffix)(__VA_ARGS__)
#define SIMD_FUNC_H(name, suffix, ...) \
    _SIMD_FUNC_H_IMPL(name, suffix)(__VA_ARGS__)

#define SIMD_FUNC_DEC_F(name, ...) \
    SIMD_FUNC_F(name, SIMD_FULL_PACK_SUFFIX, __VA_ARGS__)
#define SIMD_FUNC_DEC_H(name, ...) \
    SIMD_FUNC_H(name, SIMD_HALF_PACK_SUFFIX, __VA_ARGS__)

#define _SIMD_IMPL_FUNC_DEC_F(name, ...) \
    _CONCAT2(_ ## name ## _, _SIMD_IMPL_SUFFIX_F)(__VA_ARGS__)
#define _SIMD_IMPL_FUNC_DEC_H(name, ...) \
    _CONCAT2(_ ## name ## _, _SIMD_IMPL_SUFFIX_H)(__VA_ARGS__)

#define SIMD_FUNC_INT_F(name, size, ...) \
    SIMD_FUNC_F(name, _CONCAT2(epi, size), __VA_ARGS__)
#define SIMD_FUNC_INT_H(name, size, ...) \
    SIMD_FUNC_H(name, _CONCAT2(epi, size), __VA_ARGS__)

#define SIMD_FUNC_DEC_MASK_F(name, ...) \
    SIMD_FUNC_F(name, _CONCAT2(SIMD_FULL_PACK_SUFFIX, _mask), __VA_ARGS__)
#define SIMD_FUNC_DEC_MASK_H(name, ...) \
    SIMD_FUNC_H(name, _CONCAT2(SIMD_HALF_PACK_SUFFIX, _mask), __VA_ARGS__)

#define SIMD_FUNC_INT_MASK_F(name, size, ...) \
    SIMD_FUNC_F(name, epi ## size ## _mask, __VA_ARGS__)
#define SIMD_FUNC_INT_MASK_H(name, size, ...) \
    SIMD_FUNC_H(name, epi ## size ## _mask, __VA_ARGS__)

#if AVX512

#define _SIMD_FUNC_KMASK_F(name, ...) \
    _CONCAT2(_k ## name ## _mask, SIMD_FULL_WIDTH)(__VA_ARGS__)
#define _SIMD_FUNC_KMASK_H(name, ...) \
    _CONCAT2(_k ## name ## _mask, SIMD_HALF_WIDTH)(__VA_ARGS__)

#endif

#ifdef USE_SLEEF

#define SLEEF_FUNC_DEC_F(name, prec, ...) \
    _CONCAT5(Sleef_ ## name, _SLEEF_FULL_SUFFIX, \
    SIMD_FULL_WIDTH, _u ## prec, _SLEEF_FULL_ARCH_NAME)(__VA_ARGS__)
#define SLEEF_FUNC_DEC_H(name, prec, ...) \
    _CONCAT5(Sleef_ ## name, _SLEEF_HALF_SUFFIX, \
    SIMD_HALF_WIDTH, _u ## prec, _SLEEF_HALF_ARCH_NAME)(__VA_ARGS__)

#endif

#define SIMD_LOAD_F(ptr) \
    SIMD_FUNC_DEC_F(load, reinterpret_cast<const scalar_full_t *>(ptr))
#define SIMD_LOAD_H(ptr) \
    SIMD_FUNC_DEC_H(load, reinterpret_cast<const scalar_half_t *>(ptr))

#define SIMD_LOADU_F(ptr) \
    SIMD_FUNC_DEC_F(loadu, reinterpret_cast<const scalar_full_t *>(ptr))
#define SIMD_LOADU_H(ptr) \
    SIMD_FUNC_DEC_H(loadu, reinterpret_cast<const scalar_half_t *>(ptr))

#define SIMD_STORE_F(ptr, vec) \
    SIMD_FUNC_DEC_F(store, reinterpret_cast<scalar_full_t *>(ptr), vec)
#define SIMD_STORE_H(ptr, vec) \
    SIMD_FUNC_DEC_H(store, reinterpret_cast<scalar_half_t *>(ptr), vec)

#define SIMD_STOREU_F(ptr, vec) \
    SIMD_FUNC_DEC_F(storeu, reinterpret_cast<scalar_full_t *>(ptr), vec)
#define SIMD_STOREU_H(ptr, vec) \
    SIMD_FUNC_DEC_H(storeu, reinterpret_cast<scalar_half_t *>(ptr), vec)

#define SIMD_SET_F(...) SIMD_FUNC_DEC_F(setr, __VA_ARGS__)
#define SIMD_SET_H(...) SIMD_FUNC_DEC_H(setr, __VA_ARGS__)

#define SIMD_SET1_F(x) SIMD_FUNC_DEC_F(set1, CAST_F(x))
#define SIMD_SET1_H(x) SIMD_FUNC_DEC_H(set1, CAST_H(x))

#define SIMD_CONSTSET_F(x) SIMD_FUNC_DEC_F(set1, SC_SYM_F(x))
#define SIMD_CONSTSET_H(x) SIMD_FUNC_DEC_H(set1, SC_SYM_H(x))

/*
#if SIMD_FULL_SIZE == 32
#define SIMD_..._INT_F SIMD_..._INT32_F
#elif SIMD_FULL_SIZE == 64
#define SIMD_..._INT_F SIMD_..._INT64_F
#endif

#if SIMD_HALF_SIZE == 32
#define SIMD_..._INT_H SIMD_..._INT32_H
#elif SIMD_HALF_SIZE == 64
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

#define SIMD_LOAD_INT_F(ptr) \
    SIMD_FUNC_F(load, SIMD_FULL_INT_SUFFIX, \
    reinterpret_cast<const simd_full_int_t *>(ptr))
#define SIMD_LOAD_INT_H(ptr) \
    SIMD_FUNC_H(load, SIMD_HALF_INT_SUFFIX, \
    reinterpret_cast<const simd_half_int_t *>(ptr))

#define SIMD_LOADU_INT_F(ptr) \
    SIMD_FUNC_F(loadu, SIMD_FULL_INT_SUFFIX, \
    reinterpret_cast<const simd_full_int_t *>(ptr))
#define SIMD_LOADU_INT_H(ptr) \
    SIMD_FUNC_H(loadu, SIMD_HALF_INT_SUFFIX, \
    reinterpret_cast<const simd_half_int_t *>(ptr))

#define SIMD_STORE_INT_F(ptr, vec) \
    SIMD_FUNC_F(store, SIMD_FULL_INT_SUFFIX, \
    reinterpret_cast<simd_full_int_t *>(ptr), vec)
#define SIMD_STORE_INT_H(ptr, vec) \
    SIMD_FUNC_H(store, SIMD_HALF_INT_SUFFIX, \
    reinterpret_cast<simd_half_int_t *>(ptr), vec)

#define SIMD_STOREU_INT_F(ptr, vec) \
    SIMD_FUNC_F(storeu, SIMD_FULL_INT_SUFFIX, \
    reinterpret_cast<simd_full_int_t *>(ptr), vec)
#define SIMD_STOREU_INT_H(ptr, vec) \
    SIMD_FUNC_H(storeu, SIMD_HALF_INT_SUFFIX, \
    reinterpret_cast<simd_half_int_t *>(ptr), vec)

#define SIMD_SET_INT32_F(...) \
    SIMD_FUNC_INT_F(setr, _INT32_SIZE_SYM_F, __VA_ARGS__)
#define SIMD_SET_INT32_H(...) \
    SIMD_FUNC_INT_H(setr, _INT32_SIZE_SYM_H, __VA_ARGS__)

#define SIMD_SET_INT64_F(...) \
    SIMD_FUNC_INT_F(setr, _INT64_SIZE_SYM_F, __VA_ARGS__)
#define SIMD_SET_INT64_H(...) \
    SIMD_FUNC_INT_H(setr, _INT64_SIZE_SYM_H, __VA_ARGS__)

#define SIMD_SET1_INT32_F(x) \
    SIMD_FUNC_INT_F(set1, _INT32_SIZE_SYM_F, CAST_INT_S(x, 32))
#define SIMD_SET1_INT32_H(x) \
    SIMD_FUNC_INT_H(set1, _INT32_SIZE_SYM_H, CAST_INT_S(x, 32))

#define SIMD_SET1_INT64_F(x) \
    SIMD_FUNC_INT_F(set1, _INT64_SIZE_SYM_F, CAST_INT_S(x, 64))
#define SIMD_SET1_INT64_H(x) \
    SIMD_FUNC_INT_H(set1, _INT64_SIZE_SYM_H, CAST_INT_S(x, 64))

#define SIMD_CONSTSET_INT32_F(x) SIMD_FUNC_INT_F(set1, _INT32_SIZE_SYM_F, x)
#define SIMD_CONSTSET_INT32_H(x) SIMD_FUNC_INT_H(set1, _INT32_SIZE_SYM_H, x)

#define SIMD_CONSTSET_INT64_F(x) SIMD_FUNC_INT_F(set1, _INT64_SIZE_SYM_F, x)
#define SIMD_CONSTSET_INT64_H(x) SIMD_FUNC_INT_H(set1, _INT64_SIZE_SYM_H, x)

#if SIMD_FULL_SIZE == 32

#define SIMD_SET_INT_F SIMD_SET_INT32_F
#define SIMD_SET1_INT_F SIMD_SET1_INT32_F
#define SIMD_CONSTSET_INT_F SIMD_CONSTSET_INT32_F

#elif SIMD_FULL_SIZE == 64

#define SIMD_SET_INT_F SIMD_SET_INT64_F
#define SIMD_SET1_INT_F SIMD_SET1_INT64_F
#define SIMD_CONSTSET_INT_F SIMD_CONSTSET_INT64_F

#endif

#if SIMD_HALF_SIZE == 32

#define SIMD_SET_INT_H SIMD_SET_INT32_H
#define SIMD_SET1_INT_H SIMD_SET1_INT32_H
#define SIMD_CONSTSET_INT_H SIMD_CONSTSET_INT32_H

#elif SIMD_HALF_SIZE == 64

#define SIMD_SET_INT_H SIMD_SET_INT64_H
#define SIMD_SET1_INT_H SIMD_SET1_INT64_H
#define SIMD_CONSTSET_INT_H SIMD_CONSTSET_INT64_H

#endif

#define SIMD_ZERO_F SIMD_FUNC_DEC_F(setzero)
#define SIMD_ZERO_H SIMD_FUNC_DEC_H(setzero)

#define SIMD_ONE_F SIMD_CONSTSET_F(1.0)
#define SIMD_ONE_H SIMD_CONSTSET_H(1.0)

#define SIMD_ONEHALF_F SIMD_CONSTSET_F(0.5)
#define SIMD_ONEHALF_H SIMD_CONSTSET_H(0.5)

#define SIMD_NEGONE_F SIMD_CONSTSET_F(-1.0)
#define SIMD_NEGONE_H SIMD_CONSTSET_H(-1.0)

#define SIMD_ZERO_FI SIMD_FUNC_F(setzero, SIMD_FULL_INT_SUFFIX)
#define SIMD_ZERO_HI SIMD_FUNC_H(setzero, SIMD_HALF_INT_SUFFIX)

#define SIMD_ONE_FI SIMD_CONSTSET_INT_F(1)
#define SIMD_ONE_HI SIMD_CONSTSET_INT_H(1)

#define SIMD_NEGONE_FI SIMD_CONSTSET_INT_F(-1)
#define SIMD_NEGONE_HI SIMD_CONSTSET_INT_H(-1)

#if defined(USE_FLOATS)
#define SIMD_FULL_TO_HALF_CONV(x) x
#elif defined(USE_DOUBLES)

#if AVX512

FORCE_INLINE __m256 SIMD_FULL_TO_HALF_CONV(__m512d x) {
    const __m256d f_low = _mm512_castpd512_pd256(x);
    const __m256d f_high = _mm512_extractf64x4_pd(x, 1);

    const __m128 h_low = _mm256_cvtpd_ps(f_low);
    const __m128 h_high = _mm256_cvtpd_ps(f_high);

    return _mm256_set_m128(h_high, h_low);
}

#else
#define SIMD_FULL_TO_HALF_CONV(x) \
    SIMD_FUNC_F(_CONCAT2(cvt, SIMD_FULL_PACK_SUFFIX), SIMD_HALF_PACK_SUFFIX, x)
#endif

#endif

#define SIMD_FULL_TO_INT_CONV(x) \
    SIMD_FUNC_INT_F(_CONCAT2(cvt, SIMD_FULL_PACK_SUFFIX), SIMD_FULL_SIZE, x)
#define SIMD_HALF_TO_INT_CONV(x) \
    SIMD_FUNC_INT_H(_CONCAT2(cvt, SIMD_HALF_PACK_SUFFIX), SIMD_HALF_SIZE, x)

#define SIMD_INT_TO_FULL_CONV(x) \
    SIMD_FUNC_INT_F(_CONCAT2(cvt, SIMD_FULL_SIZE), SIMD_FULL_PACK_SUFFIX, x)
#define SIMD_INT_TO_HALF_CONV(x) \
    SIMD_FUNC_INT_H(_CONCAT2(cvt, SIMD_HALF_SIZE), SIMD_HALF_PACK_SUFFIX, x)

#define SIMD_FULL_TO_INT_CAST(x) \
    SIMD_FUNC_F(_CONCAT2(cast, SIMD_FULL_PACK_SUFFIX), \
    SIMD_FULL_INT_SUFFIX), x)
#define SIMD_HALF_TO_INT_CAST(x) \
    SIMD_FUNC_H(_CONCAT2(cast, SIMD_HALF_PACK_SUFFIX), \
    SIMD_HALF_INT_SUFFIX, x)

#define SIMD_INT_TO_FULL_CAST(x) \
    SIMD_FUNC_DEC_F(_CONCAT2(cast, SIMD_FULL_INT_SUFFIX), x)
#define SIMD_INT_TO_HALF_CAST(x) \
    SIMD_FUNC_DEC_H(_CONCAT2(cast, SIMD_HALF_INT_SUFFIX), x)

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

#define SIMD_INIT_ZERO_MASK_F SIMD_ZERO_F
#define SIMD_INIT_ZERO_MASK_H SIMD_ZERO_H

#define SIMD_INIT_ONES_MASK_F SIMD_INT_TO_FULL_CAST(SIMD_NEGONE_FI)
#define SIMD_INIT_ONES_MASK_H SIMD_INT_TO_HALF_CAST(SIMD_NEGONE_HI)

#endif

#define SIMD_ADD_F(a, b) SIMD_FUNC_DEC_F(add, a, b)
#define SIMD_ADD_H(a, b) SIMD_FUNC_DEC_H(add, a, b)

#define SIMD_SUB_F(a, b) SIMD_FUNC_DEC_F(sub, a, b)
#define SIMD_SUB_H(a, b) SIMD_FUNC_DEC_H(sub, a, b)

#define SIMD_MUL_F(a, b) SIMD_FUNC_DEC_F(mul, a, b)
#define SIMD_MUL_H(a, b) SIMD_FUNC_DEC_H(mul, a, b)

#define SIMD_DIV_F(a, b) SIMD_FUNC_DEC_F(div, a, b)
#define SIMD_DIV_H(a, b) SIMD_FUNC_DEC_H(div, a, b)

#define SIMD_DOUBLE_F(x) SIMD_ADD_F(x, x)
#define SIMD_DOUBLE_H(x) SIMD_ADD_H(x, x)

#define SIMD_HALF_F(x) SIMD_MUL_F(x, SIMD_ONEHALF_F)
#define SIMD_HALF_H(x) SIMD_MUL_H(x, SIMD_ONEHALF_H)

#define SIMD_SQUARE_F(x) SIMD_MUL_F(x, x)
#define SIMD_SQUARE_H(x) SIMD_MUL_H(x, x)

#define SIMD_RECIP_F(x) SIMD_DIV_F(SIMD_ONE_F, x)

#if defined(USE_FLOATS) && !AVX512
#undef SIMD_RECIP_F
#define SIMD_RECIP_F(x) SIMD_FUNC_DEC_F(rcp, x)
#endif

#if AVX512
#define SIMD_RECIP_H(x) SIMD_DIV_H(SIMD_ONE_H, x)
#else
#define SIMD_RECIP_H(x) SIMD_FUNC_DEC_H(rcp, x)
#endif

#if VEC_FUSED_OPS

#define SIMD_MULADD_F(a, b, c) SIMD_FUNC_DEC_F(fmadd, a, b, c)
#define SIMD_MULADD_H(a, b, c) SIMD_FUNC_DEC_H(fmadd, a, b, c)

#define SIMD_NMULADD_F(a, b, c) SIMD_FUNC_DEC_F(fnmsub, a, b, c)
#define SIMD_NMULADD_H(a, b, c) SIMD_FUNC_DEC_H(fnmsub, a, b, c)

#define SIMD_MULSUB_F(a, b, c) SIMD_FUNC_DEC_F(fmsub, a, b, c)
#define SIMD_MULSUB_H(a, b, c) SIMD_FUNC_DEC_H(fmsub, a, b, c)

#define SIMD_SUBMUL_F(a, b, c) SIMD_FUNC_DEC_F(fnmadd, a, b, c)
#define SIMD_SUBMUL_H(a, b, c) SIMD_FUNC_DEC_H(fnmadd, a, b, c)

#define SIMD_ADDXX_F(a, b, c, d) SIMD_MULADD_F(a, b, SIMD_MUL_F(c, d))
#define SIMD_ADDXX_H(a, b, c, d) SIMD_MULADD_H(a, b, SIMD_MUL_H(c, d))

#define SIMD_SUBXX_F(a, b, c, d) SIMD_MULSUB_F(a, b, SIMD_MUL_F(c, d))
#define SIMD_SUBXX_H(a, b, c, d) SIMD_MULSUB_H(a, b, SIMD_MUL_H(c, d))

#define SIMD_ADDSQ_F(a, b) SIMD_ADDXX_F(a, a, b, b)
#define SIMD_ADDSQ_H(a, b) SIMD_ADDXX_H(a, a, b, b)

#define SIMD_SUBSQ_F(a, b) SIMD_ADDXX_F(a, a, b, b)
#define SIMD_SUBSQ_H(a, b) SIMD_ADDXX_H(a, a, b, b)

#else

#define SIMD_MULADD_F(a, b, c) SIMD_ADD_F(SIMD_MUL_F(a, b), c)
#define SIMD_MULADD_H(a, b, c) SIMD_ADD_H(SIMD_MUL_H(a, b), c)

#define SIMD_MULSUB_F(a, b, c) SIMD_SUB_F(SIMD_MUL_F(a, b), c)
#define SIMD_MULSUB_H(a, b, c) SIMD_SUB_H(SIMD_MUL_H(a, b), c)

#define SIMD_SUBMUL_F(a, b, c) SIMD_SUB_F(c, SIMD_MUL_F(a, b))
#define SIMD_SUBMUL_H(a, b, c) SIMD_SUB_H(c, SIMD_MUL_H(a, b))

#define SIMD_ADDXX_F(a, b, c, d) \
    SIMD_ADD_F( \
        SIMD_MUL_F(a, b), \
        SIMD_MUL_F(c, d) \
    )
#define SIMD_ADDXX_H(a, b, c, d) \
    SIMD_ADD_H( \
        SIMD_MUL_H(a, b), \
        SIMD_MUL_H(c, d) \
    )

#define SIMD_SUBXX_F(a, b, c, d) \
    SIMD_SUB_F( \
        SIMD_MUL_F(a, b), \
        SIMD_MUL_F(c, d) \
    )
#define SIMD_SUBXX_H(a, b, c, d) \
    SIMD_SUB_H( \
        SIMD_MUL_H(a, b), \
        SIMD_MUL_H(c, d) \
    )

#define SIMD_ADDSQ_F(a, b) \
    SIMD_ADD_F( \
        SIMD_MUL_F(a, a), \
        SIMD_MUL_F(b, b) \
    )
#define SIMD_ADDSQ_H(a, b) \
    SIMD_ADD_H( \
        SIMD_MUL_H(a, a), \
        SIMD_MUL_H(b, b) \
    )

#define SIMD_SUBSQ_F(a, b) \
    SIMD_SUB_F( \
        SIMD_MUL_F(a, a), \
        SIMD_MUL_F(b, b) \
    )
#define SIMD_SUBSQ_H(a, b) \
    SIMD_SUB_H( \
        SIMD_MUL_H(a, a), \
        SIMD_MUL_H(b, b) \
    )

#endif

#define SIMD_ADD_INT32_F(a, b) SIMD_FUNC_INT_F(add, 32, a, b)
#define SIMD_ADD_INT32_H(a, b) SIMD_FUNC_INT_H(add, 32, a, b)

#define SIMD_ADD_INT64_F(a, b) SIMD_FUNC_INT_F(add, 64, a, b)
#define SIMD_ADD_INT64_H(a, b) SIMD_FUNC_INT_H(add, 64, a, b)

#if SIMD_FULL_SIZE == 32
#define SIMD_ADD_INT_F SIMD_ADD_INT32_F
#elif SIMD_FULL_SIZE == 64
#define SIMD_ADD_INT_F SIMD_ADD_INT64_F
#endif

#if SIMD_HALF_SIZE == 32
#define SIMD_ADD_INT_H SIMD_ADD_INT32_H
#elif SIMD_HALF_SIZE == 64
#define SIMD_ADD_INT_H SIMD_ADD_INT64_H
#endif

#define SIMD_SUB_INT32_F(a, b) SIMD_FUNC_INT_F(sub, 32, a, b)
#define SIMD_SUB_INT32_H(a, b) SIMD_FUNC_INT_H(sub, 32, a, b)

#define SIMD_SUB_INT64_F(a, b) SIMD_FUNC_INT_F(sub, 64, a, b)
#define SIMD_SUB_INT64_H(a, b) SIMD_FUNC_INT_H(sub, 64, a, b)

#if SIMD_FULL_SIZE == 32
#define SIMD_SUB_INT_F SIMD_SUB_INT32_F
#elif SIMD_FULL_SIZE == 64
#define SIMD_SUB_INT_F SIMD_SUB_INT64_F
#endif

#if SIMD_HALF_SIZE == 32
#define SIMD_SUB_INT_H SIMD_SUB_INT32_H
#elif SIMD_HALF_SIZE == 64
#define SIMD_SUB_INT_H SIMD_SUB_INT64_H
#endif

#define SIMD_MUL_INT32_F(a, b) SIMD_FUNC_INT_F(mul, 32, a, b)
#define SIMD_MUL_INT32_H(a, b) SIMD_FUNC_INT_H(mul, 32, a, b)

#define SIMD_MUL_INT64_F(a, b) SIMD_FUNC_INT_F(mul, 64, a, b)
#define SIMD_MUL_INT64_H(a, b) SIMD_FUNC_INT_H(mul, 64, a, b)

#if SIMD_FULL_SIZE == 32
#define SIMD_MUL_INT_F SIMD_MUL_INT32_F
#elif SIMD_FULL_SIZE == 64
#define SIMD_MUL_INT_F SIMD_MUL_INT64_F
#endif

#if SIMD_HALF_SIZE == 32
#define SIMD_MUL_INT_H SIMD_MUL_INT32_H
#elif SIMD_HALF_SIZE == 64
#define SIMD_MUL_INT_H SIMD_MUL_INT64_H
#endif

#if AVX512
#define SIMD_HORIZ_ADD_F(x) SIMD_FUNC_DEC_F(reduce_add, x)
#define SIMD_HORIZ_ADD_H(x) SIMD_FUNC_DEC_H(reduce_add, x)
#else

FORCE_INLINE float _hadd_ps128_impl(__m128 x) {
    __m128 shuf = _mm_movehl_ps(x, x);
    __m128 sum = _mm_add_ps(x, shuf);

    shuf = _mm_shuffle_ps(sum, sum, _MM_SHUFFLE(1, 1, 1, 1));
    sum = _mm_add_ss(sum, shuf);

    return _mm_cvtss_f32(sum);
}
FORCE_INLINE double _hadd_pd128_impl(__m128d x) {
    const __m128d shuf = _mm_shuffle_pd(x, x, _MM_SHUFFLE(0, 0, 0, 1));
    const __m128d sum = _mm_add_sd(x, shuf);

    return _mm_cvtsd_f64(sum);
}

FORCE_INLINE float _hadd_ps256_impl(__m256 x) {
    const __m128 low = _mm256_castps256_ps128(x);
    const __m128 high = _mm256_extractf128_ps(x, 1);

    const __m128 sum = _mm_add_ps(low, high);
    return _hadd_ps128_impl(sum);
}
FORCE_INLINE double _hadd_pd256_impl(__m256d x) {
    const __m128d low = _mm256_castpd256_pd128(x);
    const __m128d high = _mm256_extractf128_pd(x, 1);

    const __m128d sum = _mm_add_pd(low, high);
    return _hadd_pd128_impl(sum);
}

#define SIMD_HORIZ_ADD_F(x) _SIMD_IMPL_FUNC_DEC_F(hadd, x)
#define SIMD_HORIZ_ADD_H(x) _SIMD_IMPL_FUNC_DEC_H(hadd, x)

#endif

#if AVX512
#define SIMD_MASK_TOBITS_F(x) static_cast<simd_full_mask_t>(x)
#define SIMD_MASK_TOBITS_H(x) static_cast<simd_half_mask_t>(x)
#else
#define SIMD_MASK_TOBITS_F(x) SIMD_FUNC_DEC_F(movemask, x)
#define SIMD_MASK_TOBITS_H(x) SIMD_FUNC_DEC_H(movemask, x)
#endif

#if AVX512
#define SIMD_MASK_FROMBITS_F SIMD_MASK_TOBITS_F
#define SIMD_MASK_FROMBITS_H SIMD_MASK_TOBITS_H
#else

FORCE_INLINE __m128 _mfrombits_ps128_impl(int mask) {
    const __m128i mask_vec = _mm_set1_epi32(mask);

    const __m128i bit_pos = _mm_setr_epi32(1, 2, 4, 8);
    const __m128i mask_bits = _mm_and_si128(mask_vec, bit_pos);

    const __m128i cmp_res = _mm_cmpeq_epi32(mask_bits, _mm_setzero_si128());

    const __m128i vals = _mm_xor_si128(cmp_res, _mm_set1_epi32(-1));
    return _mm_castsi128_ps(vals);
}
FORCE_INLINE __m128d _mfrombits_pd128_impl(int mask) {
    const __m128i mask_vec = _mm_set1_epi64x(mask);

    const __m128i bit_pos = _mm_setr_epi64x(1, 2);
    const __m128i mask_bits = _mm_and_si128(mask_vec, bit_pos);

    const __m128i cmp_res = _mm_cmpeq_epi64(mask_bits, _mm_setzero_si128());

    const __m128i vals = _mm_xor_si128(cmp_res, _mm_set1_epi64x(-1));
    return _mm_castsi128_pd(vals);
}

FORCE_INLINE __m256 _mfrombits_ps256_impl(int mask) {
    const __m256i mask_vec = _mm256_set1_epi32(mask);

    const __m256i bit_pos = _mm256_setr_epi32(1, 2, 4, 8, 16, 32, 64, 128);
    const __m256i mask_bits = _mm256_and_si256(mask_vec, bit_pos);

    const __m256i cmp_res = _mm256_cmpeq_epi32(mask_bits,
        _mm256_setzero_si256());

    const __m256i vals = _mm256_xor_si256(cmp_res, _mm256_set1_epi32(-1));
    return _mm256_castsi256_ps(vals);
}
FORCE_INLINE __m256d _mfrombits_pd256_impl(int mask) {
    const __m256i mask_vec = _mm256_set1_epi64x(mask);

    const __m256i bit_pos = _mm256_setr_epi64x(1, 2, 4, 8);
    const __m256i mask_bits = _mm256_and_si256(mask_vec, bit_pos);

    const __m256i cmp_res = _mm256_cmpeq_epi64(mask_bits,
        _mm256_setzero_si256());

    const __m256i vals = _mm256_xor_si256(cmp_res, _mm256_set1_epi64x(-1));
    return _mm256_castsi256_pd(vals);
}

#define SIMD_MASK_FROMBITS_F(x) _SIMD_IMPL_FUNC_DEC_F(mfrombits, x)
#define SIMD_MASK_FROMBITS_H(x) _SIMD_IMPL_FUNC_DEC_H(mfrombits, x)

#endif

#if AVX512

#define SIMD_MASK_ALLZERO_F(x) (x == SIMD_ZERO_LANES_F)
#define SIMD_MASK_ALLZERO_H(x) (x == SIMD_ZERO_LANES_H)

#define SIMD_MASK_ALLONES_F(x) (x == SIMD_ONES_LANES_F)
#define SIMD_MASK_ALLONES_H(x) (x == SIMD_ONES_LANES_H)

#define SIMD_MASK_NONZERO_F(x) (x != SIMD_ZERO_LANES_F)
#define SIMD_MASK_NONZERO_H(x) (x != SIMD_ZERO_LANES_H)

#elif AVX2

#define _SIMD_TESTZ_F(a, b) SIMD_FUNC_DEC_F(testz, a, b)
#define _SIMD_TESTZ_H(a, b) SIMD_FUNC_DEC_H(testz, a, b)

#define _SIMD_TESTC_F(a, b) SIMD_FUNC_DEC_F(testc, a, b)
#define _SIMD_TESTC_H(a, b) SIMD_FUNC_DEC_H(testc, a, b)

#define SIMD_MASK_ALLZERO_F(x) (_SIMD_TESTZ_F(x, x) == 1)
#define SIMD_MASK_ALLZERO_H(x) (_SIMD_TESTZ_H(x, x) == 1)

#define SIMD_MASK_ALLONES_F(x) (_SIMD_TESTC_F(x, SIMD_INIT_ONES_MASK_F) != 0)
#define SIMD_MASK_ALLONES_H(x) (_SIMD_TESTC_H(x, SIMD_INIT_ONES_MASK_H) != 0)

#define SIMD_MASK_NONZERO_F(x) (_SIMD_TESTZ_F(x, x) != 0)
#define SIMD_MASK_NONZERO_H(x) (_SIMD_TESTZ_H(x, x) != 0)

#elif SSE

#define SIMD_MASK_ALLZERO_F(x) (SIMD_MASK_TOBITS_F(x) == SIMD_ZERO_LANES_F)
#define SIMD_MASK_ALLZERO_H(x) (SIMD_MASK_TOBITS_H(x) == SIMD_ZERO_LANES_H)

#define SIMD_MASK_ALLONES_F(x) (SIMD_MASK_TOBITS_F(x) == SIMD_ONES_LANES_F)
#define SIMD_MASK_ALLONES_H(x) (SIMD_MASK_TOBITS_H(x) == SIMD_ONES_LANES_H)

#define SIMD_MASK_NONZERO_F(x) (SIMD_MASK_TOBITS_F(x) != SIMD_ZERO_LANES_F)
#define SIMD_MASK_NONZERO_H(x) (SIMD_MASK_TOBITS_H(x) != SIMD_ZERO_LANES_H)

#endif

#define SIMD_AND_F(a, b) SIMD_FUNC_DEC_F(and, a, b)
#define SIMD_AND_H(a, b) SIMD_FUNC_DEC_H(and, a, b)

#define SIMD_AND_INT_F(a, b) SIMD_FUNC_F(and, SIMD_FULL_INT_SUFFIX, a, b)
#define SIMD_AND_INT_H(a, b) SIMD_FUNC_H(and, SIMD_HALF_INT_SUFFIX, a, b)

#if AVX512
#define SIMD_MASK_AND_F(x, mask) SIMD_FUNC_DEC_F(maskz_mov, mask, x)
#define SIMD_MASK_AND_H(x, mask) SIMD_FUNC_DEC_H(maskz_mov, mask, x)
#else
#define SIMD_MASK_AND_F SIMD_AND_F
#define SIMD_MASK_AND_H SIMD_AND_H
#endif

#if AVX512
#define SIMD_AND_MASK_F(a, b) _SIMD_FUNC_KMASK_F(and, a, b)
#define SIMD_AND_MASK_H(a, b) _SIMD_FUNC_KMASK_H(and, a, b)
#else
#define SIMD_AND_MASK_F SIMD_AND_F
#define SIMD_AND_MASK_H SIMD_AND_H
#endif

#define SIMD_OR_F(a, b) SIMD_FUNC_DEC_F(or, a, b)
#define SIMD_OR_H(a, b) SIMD_FUNC_DEC_H(or, a, b)

#define SIMD_OR_INT_F(a, b) SIMD_FUNC_F(or, SIMD_FULL_INT_SUFFIX, a, b)
#define SIMD_OR_INT_H(a, b) SIMD_FUNC_H(or, SIMD_HALF_INT_SUFFIX, a, b)

#if AVX512
#define SIMD_MASK_OR_F(x, mask) SIMD_FUNC_DEC_F(mask_mov, x, mask, SIMD_ONE_F)
#define SIMD_MASK_OR_H(x, mask) SIMD_FUNC_DEC_H(mask_mov, x, mask, SIMD_ONE_H)
#else
#define SIMD_MASK_OR_F SIMD_OR_F
#define SIMD_MASK_OR_H SIMD_OR_H
#endif

#if AVX512
#define SIMD_OR_MASK_F(a, b) _SIMD_FUNC_KMASK_F(or, a, b)
#define SIMD_OR_MASK_H(a, b) _SIMD_FUNC_KMASK_H(or, a, b)
#else
#define SIMD_OR_MASK_F SIMD_OR_F
#define SIMD_OR_MASK_H SIMD_OR_H
#endif

#define SIMD_XOR_F(a, b) SIMD_FUNC_DEC_F(xor, a, b)
#define SIMD_XOR_H(a, b) SIMD_FUNC_DEC_H(xor, a, b)

#define SIMD_XOR_INT_F(a, b) SIMD_FUNC_F(xor, SIMD_FULL_INT_SUFFIX, a, b)
#define SIMD_XOR_INT_H(a, b) SIMD_FUNC_H(xor, SIMD_HALF_INT_SUFFIX, a, b)

#if AVX512

#define SIMD_MASK_XOR_F(x, mask) \
    SIMD_MASK_OR_F(SIMD_ONE_F, _kxor_mask16(SIMD_IS0_F(x), mask))
#define SIMD_MASK_XOR_H(x, mask) \
    SIMD_MASK_OR_H(SIMD_ONE_H, _kxor_mask16(SIMD_IS0_H(x), mask))

#else
#define SIMD_MASK_XOR_F SIMD_XOR_F
#define SIMD_MASK_XOR_H SIMD_XOR_H
#endif

#if AVX512
#define SIMD_XOR_MASK_F(a, b) _SIMD_FUNC_KMASK_F(xor, a, b)
#define SIMD_XOR_MASK_H(a, b) _SIMD_FUNC_KMASK_H(xor, a, b)
#else
#define SIMD_XOR_MASK_F SIMD_XOR_F
#define SIMD_XOR_MASK_H SIMD_XOR_H
#endif

#if AVX512

#define SIMD_NOT_MASK_F(a, b) _SIMD_FUNC_KMASK_F(not, a, b)
#define SIMD_NOT_MASK_H(a, b) _SIMD_FUNC_KMASK_H(not, a, b)

#define SIMD_NOT_INT_MASK_F SIMD_NOT_MASK_F
#define SIMD_NOT_INT_MASK_H SIMD_NOT_MASK_H

#else

#define SIMD_NOT_MASK_F(x) SIMD_XOR_F(x, SIMD_INIT_ONES_MASK_F)
#define SIMD_NOT_MASK_H(x) SIMD_XOR_H(x, SIMD_INIT_ONES_MASK_H)

#define SIMD_NOT_INT_MASK_F(x) SIMD_XOR_INT_F(x, SIMD_NEGONE_FI)
#define SIMD_NOT_INT_MASK_H(x) SIMD_XOR_INT_H(x, SIMD_NEGONE_HI)

#endif

#define SIMD_ANDNOT_F(a, b) SIMD_FUNC_DEC_F(andnot, a, b)
#define SIMD_ANDNOT_H(a, b) SIMD_FUNC_DEC_H(andnot, a, b)

#define SIMD_ANDNOT_INT_F(a, b) SIMD_FUNC_F(andnot, SIMD_FULL_INT_SUFFIX, a, b)
#define SIMD_ANDNOT_INT_H(a, b) SIMD_FUNC_H(andnot, SIMD_HALF_INT_SUFFIX, a, b)

#if AVX512

#define SIMD_MASK_ANDNOT_F(x, mask) \
    SIMD_MASK_OR_F(SIMD_ONE_F, _kandn_mask16(SIMD_NOT0_F(x), mask))
#define SIMD_MASK_ANDNOT_H(x, mask) \
    SIMD_MASK_OR_H(SIMD_ONE_H, _kandn_mask16(SIMD_NOT0_H(x), mask))

#else
#define SIMD_MASK_ANDNOT_F SIMD_ANDNOT_F
#define SIMD_MASK_ANDNOT_H SIMD_ANDNOT_H
#endif

#if AVX512
#define SIMD_ANDNOT_MASK_F(a, b) _SIMD_FUNC_KMASK_F(andn, a, b)
#define SIMD_ANDNOT_MASK_H(a, b) _SIMD_FUNC_KMASK_H(andn, a, b)
#else
#define SIMD_ANDNOT_MASK_F SIMD_ANDNOT_F
#define SIMD_ANDNOT_MASK_H SIMD_ANDNOT_H
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

#define SIMD_IS0_F(x) SIMD_CMP_EQ_F(x, SIMD_ZERO_F)
#define SIMD_IS0_H(x) SIMD_CMP_EQ_H(x, SIMD_ZERO_H)

#define SIMD_ISNOT0_F(x) SIMD_CMP_NEQ_F(x, SIMD_ZERO_F)
#define SIMD_ISNOT0_H(x) SIMD_CMP_NEQ_H(x, SIMD_ZERO_H)

#define SIMD_ISPOS_F(x) SIMD_CMP_GT_F(x, SIMD_ZERO_F)
#define SIMD_ISPOS_H(x) SIMD_CMP_GT_H(x, SIMD_ZERO_H)

#define SIMD_ISNEG_F(x) SIMD_CMP_LT_F(x, SIMD_ZERO_F)
#define SIMD_ISNEG_H(x) SIMD_CMP_LT_H(x, SIMD_ZERO_H)

#define SIMD_ISPOS0_F(x) SIMD_CMP_GE_F(x, SIMD_ZERO_F)
#define SIMD_ISPOS0_H(x) SIMD_CMP_GE_H(x, SIMD_ZERO_H)

#define SIMD_ISNEG0_F(x) SIMD_CMP_LE_F(x, SIMD_ZERO_F)
#define SIMD_ISNEG0_H(x) SIMD_CMP_LE_H(x, SIMD_ZERO_H)

#if AVX512

#define SIMD_CMP_INT32_EQ_F(a, b) SIMD_FUNC_INT_MASK_F(cmpeq, 32, a, b)
#define SIMD_CMP_INT32_EQ_H(a, b) SIMD_FUNC_INT_MASK_H(cmpeq, 32, a, b)

#define SIMD_CMP_INT64_EQ_F(a, b) SIMD_FUNC_INT_MASK_F(cmpeq, 64, a, b)
#define SIMD_CMP_INT64_EQ_H(a, b) SIMD_FUNC_INT_MASK_H(cmpeq, 64, a, b)

#define SIMD_CMP_INT32_NEQ_F(a, b) SIMD_FUNC_INT_MASK_F(cmpneq, 32, a, b)
#define SIMD_CMP_INT32_NEQ_H(a, b) SIMD_FUNC_INT_MASK_H(cmpneq, 32, a, b)

#define SIMD_CMP_INT64_NEQ_F(a, b) SIMD_FUNC_INT_MASK_F(cmpneq, 64, a, b)
#define SIMD_CMP_INT64_NEQ_H(a, b) SIMD_FUNC_INT_MASK_H(cmpneq, 64, a, b)

#define SIMD_CMP_INT32_LT_F(a, b) SIMD_FUNC_INT_MASK_F(cmplt, 32, a, b)
#define SIMD_CMP_INT32_LT_H(a, b) SIMD_FUNC_INT_MASK_H(cmplt, 32, a, b)

#define SIMD_CMP_INT64_LT_F(a, b) SIMD_FUNC_INT_MASK_F(cmplt, 64, a, b)
#define SIMD_CMP_INT64_LT_H(a, b) SIMD_FUNC_INT_MASK_H(cmplt, 64, a, b)

#define SIMD_CMP_INT32_GT_F(a, b) SIMD_FUNC_INT_MASK_F(cmpgt, 32, a, b)
#define SIMD_CMP_INT32_GT_H(a, b) SIMD_FUNC_INT_MASK_H(cmpgt, 32, a, b)

#define SIMD_CMP_INT64_GT_F(a, b) SIMD_FUNC_INT_MASK_F(cmpgt, 64, a, b)
#define SIMD_CMP_INT64_GT_H(a, b) SIMD_FUNC_INT_MASK_H(cmpgt, 64, a, b)

#define SIMD_CMP_INT32_LE_F(a, b) SIMD_FUNC_INT_MASK_F(cmple, 32, a, b)
#define SIMD_CMP_INT32_LE_H(a, b) SIMD_FUNC_INT_MASK_H(cmple, 32, a, b)

#define SIMD_CMP_INT64_LE_F(a, b) SIMD_FUNC_INT_MASK_F(cmple, 64, a, b)
#define SIMD_CMP_INT64_LE_H(a, b) SIMD_FUNC_INT_MASK_H(cmple, 64, a, b)

#define SIMD_CMP_INT32_GE_F(a, b) SIMD_FUNC_INT_MASK_F(cmpge, 32, a, b)
#define SIMD_CMP_INT32_GE_H(a, b) SIMD_FUNC_INT_MASK_H(cmpge, 32, a, b)

#define SIMD_CMP_INT64_GE_F(a, b) SIMD_FUNC_INT_MASK_F(cmpge, 64, a, b)
#define SIMD_CMP_INT64_GE_H(a, b) SIMD_FUNC_INT_MASK_H(cmpge, 64, a, b)

#else

#define SIMD_CMP_INT32_EQ_F(a, b) SIMD_FUNC_INT_F(cmpeq, 32, a, b)
#define SIMD_CMP_INT32_EQ_H(a, b) SIMD_FUNC_INT_H(cmpeq, 32, a, b)

#define SIMD_CMP_INT64_EQ_F(a, b) SIMD_FUNC_INT_F(cmpeq, 64, a, b)
#define SIMD_CMP_INT64_EQ_H(a, b) SIMD_FUNC_INT_H(cmpeq, 64, a, b)

#define SIMD_CMP_INT32_NEQ_F(a, b) \
    SIMD_NOT_INT_MASK_F(SIMD_FUNC_INT_F(cmpneq, 32, a, b))
#define SIMD_CMP_INT32_NEQ_H(a, b) \
    SIMD_NOT_INT_MASK_H(SIMD_FUNC_INT_H(cmpneq, 32, a, b))

#define SIMD_CMP_INT64_NEQ_F(a, b) \
    SIMD_NOT_INT_MASK_F(SIMD_FUNC_INT_F(cmpneq, 64, a, b))
#define SIMD_CMP_INT64_NEQ_H(a, b) \
    SIMD_NOT_INT_MASK_H(SIMD_FUNC_INT_H(cmpneq, 64, a, b))

#define SIMD_CMP_INT32_LT_F(a, b) SIMD_FUNC_INT_F(cmpgt, 32, b, a)
#define SIMD_CMP_INT32_LT_H(a, b) SIMD_FUNC_INT_H(cmpgt, 32, b, a)

#define SIMD_CMP_INT64_LT_F(a, b) SIMD_FUNC_INT_F(cmpgt, 64, b, a)
#define SIMD_CMP_INT64_LT_H(a, b) SIMD_FUNC_INT_H(cmpgt, 64, b, a)

#define SIMD_CMP_INT32_GT_F(a, b) SIMD_FUNC_INT_F(cmpgt, 32, a, b)
#define SIMD_CMP_INT32_GT_H(a, b) SIMD_FUNC_INT_H(cmpgt, 32, a, b)

#define SIMD_CMP_INT64_GT_F(a, b) SIMD_FUNC_INT_F(cmpgt, 64, a, b)
#define SIMD_CMP_INT64_GT_H(a, b) SIMD_FUNC_INT_H(cmpgt, 64, a, b)

#define SIMD_CMP_INT32_LE_F(a, b) \
    SIMD_NOT_INT_MASK_F(SIMD_FUNC_INT_F(cmpgt, 32, a, b))
#define SIMD_CMP_INT32_LE_H(a, b) \
    SIMD_NOT_INT_MASK_H(SIMD_FUNC_INT_H(cmpgt, 32, a, b))

#define SIMD_CMP_INT64_LE_F(a, b) \
    SIMD_NOT_INT_MASK_F(SIMD_FUNC_INT_F(cmpgt, 64, a, b))
#define SIMD_CMP_INT64_LE_H(a, b) \
    SIMD_NOT_INT_MASK_H(SIMD_FUNC_INT_H(cmpgt, 64, a, b))

#define SIMD_CMP_INT32_GE_F(a, b) \
    SIMD_NOT_INT_MASK_F(SIMD_FUNC_INT_F(cmpge, 32, b, a))
#define SIMD_CMP_INT32_GE_H(a, b) \
    SIMD_NOT_INT_MASK_H(SIMD_FUNC_INT_H(cmpge, 32, b, a))

#define SIMD_CMP_INT64_GE_F(a, b) \
    SIMD_NOT_INT_MASK_F(SIMD_FUNC_INT_F(cmpge, 64, b, a))
#define SIMD_CMP_INT64_GE_H(a, b) \
    SIMD_NOT_INT_MASK_H(SIMD_FUNC_INT_H(cmpge, 64, b, a))

#endif

#if SIMD_FULL_SIZE == 32

#define SIMD_CMP_INT_EQ_F SIMD_CMP_INT32_EQ_F
#define SIMD_CMP_INT_NEQ_F SIMD_CMP_INT32_NEQ_F
#define SIMD_CMP_INT_LT_F SIMD_CMP_INT32_LT_F
#define SIMD_CMP_INT_GT_F SIMD_CMP_INT32_GT_F
#define SIMD_CMP_INT_LE_F SIMD_CMP_INT32_LE_F
#define SIMD_CMP_INT_GE_F SIMD_CMP_INT32_GE_F

#elif SIMD_FULL_SIZE == 64

#define SIMD_CMP_INT_EQ_F SIMD_CMP_INT64_EQ_F
#define SIMD_CMP_INT_NEQ_F SIMD_CMP_INT64_NEQ_F
#define SIMD_CMP_INT_LT_F SIMD_CMP_INT64_LT_F
#define SIMD_CMP_INT_GT_F SIMD_CMP_INT64_GT_F
#define SIMD_CMP_INT_LE_F SIMD_CMP_INT64_LE_F
#define SIMD_CMP_INT_GE_F SIMD_CMP_INT64_GE_F

#endif

#if SIMD_HALF_SIZE == 32

#define SIMD_CMP_INT_EQ_H SIMD_CMP_INT32_EQ_H
#define SIMD_CMP_INT_NEQ_H SIMD_CMP_INT32_NEQ_H
#define SIMD_CMP_INT_LT_H SIMD_CMP_INT32_LT_H
#define SIMD_CMP_INT_GT_H SIMD_CMP_INT32_GT_H
#define SIMD_CMP_INT_LE_H SIMD_CMP_INT32_LE_H
#define SIMD_CMP_INT_GE_H SIMD_CMP_INT32_GE_H

#elif SIMD_HALF_SIZE == 64

#define SIMD_CMP_INT_EQ_H SIMD_CMP_INT64_EQ_H
#define SIMD_CMP_INT_NEQ_H SIMD_CMP_INT64_NEQ_H
#define SIMD_CMP_INT_LT_H SIMD_CMP_INT64_LT_H
#define SIMD_CMP_INT_GT_H SIMD_CMP_INT64_GT_H
#define SIMD_CMP_INT_LE_H SIMD_CMP_INT64_LE_H
#define SIMD_CMP_INT_GE_H SIMD_CMP_INT64_GE_H

#endif

#define SIMD_MIN_F(a, b) SIMD_FUNC_DEC_F(min, a, b)
#define SIMD_MIN_H(a, b) SIMD_FUNC_DEC_H(min, a, b)

#define SIMD_MAX_F(a, b) SIMD_FUNC_DEC_F(max, a, b)
#define SIMD_MAX_H(a, b) SIMD_FUNC_DEC_H(max, a, b)

#define SIMD_CLAMP_F(x, a, b) SIMD_MIN_F(SIMD_MAX_F(x, a), b)
#define SIMD_CLAMP_H(x, a, b) SIMD_MIN_H(SIMD_MAX_H(x, a), b)

#define SIMD_NEG_F(x) SIMD_XOR_F(x, SIMD_CONSTSET_F(-0.0))
#define SIMD_NEG_H(x) SIMD_XOR_H(x, SIMD_CONSTSET_H(-0.0))

#define SIMD_ABS_F(x) SIMD_ANDNOT_F(SIMD_CONSTSET_F(-0.0), x)
#define SIMD_ABS_H(x) SIMD_ANDNOT_H(SIMD_CONSTSET_H(-0.0), x)

#if AVX512

FORCE_INLINE __m512 _floor_ps512_impl(__m512 x) {
    __m512i tmp = _mm512_cvttps_epi32(x);
    const __m512 floored = _mm512_cvtepi32_ps(tmp);

    const __mmask16 needsAdj = _mm512_cmp_ps_mask(floored, x, _CMP_GT_OQ);
    tmp = _mm512_mask_sub_epi32(tmp, needsAdj, tmp, _mm512_set1_epi32(1));

    return _mm512_cvtepi32_ps(tmp);
}
FORCE_INLINE __m512d _floor_pd512_impl(__m512d x) {
    __m512i tmp = _mm512_cvttpd_epi64(x);
    const __m512d floored = _mm512_cvtepi64_pd(tmp);

    const __mmask8 needsAdj = _mm512_cmp_pd_mask(floored, x, _CMP_GT_OQ);
    tmp = _mm512_mask_sub_epi64(tmp, needsAdj, tmp, _mm512_set1_epi64(1));

    return _mm512_cvtepi64_pd(tmp);
}

#if SIMD_FULL_ARCH_WIDTH == 512
#define SIMD_FLOOR_F(x) _SIMD_IMPL_FUNC_DEC_F(floor, x)
#else
#define SIMD_FLOOR_F(x) SIMD_FUNC_DEC_F(floor, x)
#endif

#if SIMD_HALF_ARCH_WIDTH == 512
#define SIMD_FLOOR_H(x) _SIMD_IMPL_FUNC_DEC_H(floor, x)
#else
#define SIMD_FLOOR_H(x) SIMD_FUNC_DEC_H(floor, x)
#endif

#else
#define SIMD_FLOOR_F(x) SIMD_FUNC_DEC_F(floor, x)
#define SIMD_FLOOR_H(x) SIMD_FUNC_DEC_H(floor, x)
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
#define SIMD_ADD_MASK_F(a, b, mask) SIMD_ADD_F(a, SIMD_AND_F(b, mask))
#define SIMD_ADD_MASK_H(a, b, mask) SIMD_ADD_H(a, SIMD_AND_H(b, mask))
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

#if SIMD_FULL_SIZE == 32
#define SIMD_ADD_INT_MASK_F SIMD_ADD_INT64_MASK_H
#elif SIMD_FULL_SIZE == 64
#define SIMD_ADD_INT_MASK_F SIMD_ADD_INT64_MASK_H
#endif

#if SIMD_HALF_SIZE == 32
#define SIMD_ADD_INT_MASK_H SIMD_ADD_INT32_MASK_H
#elif SIMD_HALF_SIZE == 64
#define SIMD_ADD_INT_MASK_H SIMD_ADD_INT32_MASK_H
#endif

#else

#define SIMD_ADD_INT_MASK_F(a, b, mask) \
    SIMD_ADD_INT_F(a, SIMD_AND_INT_H(b, mask))
#define SIMD_ADD_INT_MASK_H(a, b, mask) \
    SIMD_ADD_INT_H(a, SIMD_AND_INT_H(b, mask))

#endif

#if AVX512
#define SIMD_SUB_MASK_F(a, b, mask) SIMD_FUNC_DEC_F(mask_sub, a, mask, a, b)
#define SIMD_SUB_MASK_H(a, b, mask) SIMD_FUNC_DEC_H(mask_sub, a, mask, a, b)
#else
#define SIMD_SUB_MASK_F(a, b, mask) SIMD_SUB_F(a, SIMD_AND_F(b, mask))
#define SIMD_SUB_MASK_H(a, b, mask) SIMD_SUB_H(a, SIMD_AND_H(b, mask))
#endif

#if AVX512
#define SIMD_MUL_MASK_F(a, b, mask) SIMD_FUNC_DEC_F(mask_mul, a, mask, a, b)
#define SIMD_MUL_MASK_H(a, b, mask) SIMD_FUNC_DEC_H(mask_mul, a, mask, a, b)
#else
#define SIMD_MUL_MASK_F(a, b, mask) SIMD_BLEND_F(a, SIMD_MUL_F(a, b), mask)
#define SIMD_MUL_MASK_H(a, b, mask) SIMD_BLEND_H(a, SIMD_MUL_H(a, b), mask)
#endif

#if AVX512
#define SIMD_FULL_MASK_TO_HALF(x) SIMD_MASK_AND_H(SIMD_ONE_H, x)
#else
#define SIMD_FULL_MASK_TO_HALF SIMD_FULL_TO_HALF_CONV
#endif

#if AVX512
#define SIMD_FULL_MASK_TO_INT_MASK(x) static_cast<simd_full_mask_t>(x)
#define SIMD_HALF_MASK_TO_INT_MASK(x) static_cast<simd_half_mask_t>(x)
#else
#define SIMD_FULL_MASK_TO_INT_MASK SIMD_FULL_TO_INT_CAST
#define SIMD_HALF_MASK_TO_INT_MASK SIMD_HALF_TO_INT_CAST
#endif

#define SIMD_SQRT_F(x) SIMD_FUNC_DEC_F(sqrt, x)
#define SIMD_SQRT_H(x) SIMD_FUNC_DEC_H(sqrt, x)

#define SIMD_RSQRT_F(x) SIMD_DIV_F(SIMD_ONE_F, SIMD_SQRT_F(x))

#if defined(USE_FLOATS) && !AVX512
#undef SIMD_RSQRT_F
#define SIMD_RSQRT_F(x) SIMD_FUNC_DEC_F(rsqrt, x)
#endif

#if AVX512
#define SIMD_RSQRT_H(x) SIMD_DIV_H(SIMD_ONE_H, SIMD_SQRT_H(x))
#else
#define SIMD_RSQRT_H(x) SIMD_FUNC_DEC_H(rsqrt, x)
#endif

#ifdef USE_SLEEF

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

FORCE_INLINE simd_full_2_t SIMD_SINCOS_F(simd_full_t x) {
    auto val = _SIMD_SINCOS_F_IMPL(x);
    return reinterpret_cast<simd_full_2_t &>(val);
}
FORCE_INLINE simd_half_2_t SIMD_SINCOS_H(simd_half_t x) {
    auto val = _SIMD_SINCOS_H_IMPL(x);
    return reinterpret_cast<simd_half_2_t &>(val);
}

#else

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

FORCE_INLINE simd_full_2_t SIMD_SINCOS_F(simd_full_t x) {
    simd_full_2_t out;
    out.x = _SIMD_SINCOS_F_IMPL(&out.y, x);
    return out;
}
FORCE_INLINE simd_half_2_t SIMD_SINCOS_H(simd_half_t x) {
    simd_half_2_t out;
    out.x = _SIMD_SINCOS_H_IMPL(&out.y, x);
    return out;
}

#endif

#if defined(USE_FLOATS)
#define _SIMD_GATHER_SCALE_F 4
#define _SIMD_GATHER_SCALE_H _SIMD_GATHER_SCALE_F
#elif defined(USE_DOUBLES)
#define _SIMD_GATHER_SCALE_F 8
#define _SIMD_GATHER_SCALE_H 4
#endif

#if AVX2 || AVX512

#if SIMD_FULL_ARCH_WIDTH == 512

#define SIMD_GATHER_INT32_F(ptr, idx) \
    SIMD_FUNC_DEC_H(i32gather, idx, ptr, _SIMD_GATHER_SCALE_F)
#define SIMD_GATHER_INT64_F(ptr, idx) \
    SIMD_FUNC_DEC_H(i64gather, idx, ptr, _SIMD_GATHER_SCALE_F)

#else

#define SIMD_GATHER_INT32_F(ptr, idx) \
    SIMD_FUNC_DEC_H(i32gather, ptr, idx, _SIMD_GATHER_SCALE_F)
#define SIMD_GATHER_INT64_F(ptr, idx) \
    SIMD_FUNC_DEC_H(i64gather, ptr, idx, _SIMD_GATHER_SCALE_F)

#endif

#if SIMD_HALF_ARCH_WIDTH == 512

#define SIMD_GATHER_INT32_H(ptr, idx) \
    SIMD_FUNC_DEC_H(i32gather, idx, ptr, _SIMD_GATHER_SCALE_H)
#define SIMD_GATHER_INT64_H(ptr, idx) \
    SIMD_FUNC_DEC_H(i64gather, idx, ptr, _SIMD_GATHER_SCALE_H)

#else

#define SIMD_GATHER_INT32_H(ptr, idx) \
    SIMD_FUNC_DEC_H(i32gather, ptr, idx, _SIMD_GATHER_SCALE_H)
#define SIMD_GATHER_INT64_H(ptr, idx) \
    SIMD_FUNC_DEC_H(i64gather, ptr, idx, _SIMD_GATHER_SCALE_H)

#endif

#if SIMD_FULL_SIZE == 32
#define SIMD_GATHER_F SIMD_GATHER_INT32_F
#elif SIMD_FULL_SIZE == 64
#define SIMD_GATHER_F SIMD_GATHER_INT64_F
#endif

#if SIMD_HALF_SIZE == 32
#define SIMD_GATHER_H SIMD_GATHER_INT32_H
#elif SIMD_HALF_SIZE == 64
#define SIMD_GATHER_H SIMD_GATHER_INT64_H
#endif

#else

FORCE_INLINE simd_full_t SIMD_GATHER_F(
    const scalar_full_t *values,
    simd_full_int_t idx
) {
    alignas(SIMD_FULL_ALIGNMENT) int idx_arr[SIMD_FULL_WIDTH];
    alignas(SIMD_FULL_ALIGNMENT) scalar_full_t gather_arr[SIMD_FULL_WIDTH];

    SIMD_STORE_INT_F(idx_arr, idx);

    for (int i = 0; i < SIMD_FULL_WIDTH; i++) {
        gather_arr[i] = values[idx_arr[i]];
    }

    return SIMD_LOAD_F(gather_arr);
}
FORCE_INLINE simd_half_t SIMD_GATHER_H(
    const scalar_half_t *values,
    simd_half_int_t idx
) {
    alignas(SIMD_HALF_ALIGNMENT) int idx_arr[SIMD_HALF_WIDTH];
    alignas(SIMD_HALF_ALIGNMENT) scalar_half_t gather_arr[SIMD_HALF_WIDTH];

    SIMD_STORE_INT_H(idx_arr, idx);

    for (int i = 0; i < SIMD_HALF_WIDTH; i++) {
        gather_arr[i] = values[idx_arr[i]];
    }

    return SIMD_LOAD_H(gather_arr);
}

#endif

#define SIMD_PACK_USAT_INT16_H(a, x) SIMD_FUNC_INT_H(packus, 16, a, x)
#define SIMD_PACK_USAT_INT32_H(a, x) SIMD_FUNC_INT_H(packus, 32, a, x)
#define SIMD_SHUFFLE_INT8_H(a, x) SIMD_FUNC_INT_H(packus, 8, a, x)

#endif