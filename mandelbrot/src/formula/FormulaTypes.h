#pragma once

#define FORMULA_FUNC(name) _CONCAT3(formula_, name, Power)
#define DERIVATIVE_FUNC(name) _CONCAT3(derivative_, name, Power)

#define _USE_FUSED_OPS 0

#if defined(_FORMULA_SCALAR)
#include "scalar/ScalarTypes.h"

typedef scalar_full_t number_t;
typedef scalar_full_2_t number_2_t;
typedef scalar_full_t number_param_t;

#define NUM_CONST SC_SYM_F
#define NUM_VAR CAST_F

#define NUM_ADD(a, b) (CAST_F(a) + (b))
#define NUM_SUB(a, b) (CAST_F(a) - (b))
#define NUM_MUL(a, b) (CAST_F(a) * (b))
#define NUM_DIV(a, b) (CAST_F(a) / (b))

#define NUM_DOUBLE(x) (CAST_F(x) * SC_SYM_F(2.0))
#define NUM_HALF(x) (CAST_F(x) * SC_SYM_F(0.5))
#define NUM_SQUARE(x) (CAST_F(x) * (x))

#define NUM_MULADD(a, b, c) (CAST_F(a) * (b) + (c))
#define NUM_MULSUB(a, b, c) (CAST_F(a) * (b) - (c))

#define NUM_ADDXX(a, b, c, d) (CAST_F(a) * (b) + (c) * (d))
#define NUM_SUBXX(a, b, c, d) (CAST_F(a) * (b) - (c) * (d))

#define NUM_ADDSQ(a, b) (CAST_F(a) * (a) + (b) * (b))
#define NUM_SUBSQ(a, b) (CAST_F(a) * (a) - (b) * (b))

#define NUM_NEG(x) -(x)
#define NUM_ABS ABS_F
#define NUM_SGN SGN_F

#define NUM_SQRT SQRT_F
#define NUM_POW POW_F

#define NUM_SIN SIN_F
#define NUM_COS COS_F
#define NUM_ATAN2 ATAN2_F
#define NUM_SINCOS SINCOS_F

#elif defined(_FORMULA_VECTOR)
#include "vector/VectorTypes.h"

#undef _USE_FUSED_OPS
#define _USE_FUSED_OPS VEC_FUSED_OPS

typedef simd_full_t number_t;
typedef simd_full_2_t number_2_t;
typedef simd_full_t number_param_t;

#define NUM_CONST SIMD_CONSTSET_F
#define NUM_VAR SIMD_SET1_F

#define NUM_ADD SIMD_ADD_F
#define NUM_SUB SIMD_SUB_F
#define NUM_MUL SIMD_MUL_F
#define NUM_DIV SIMD_DIV_F

#define NUM_DOUBLE SIMD_DOUBLE_F
#define NUM_HALF SIMD_HALF_F
#define NUM_SQUARE SIMD_SQUARE_F

#define NUM_MULADD SIMD_MULADD_F
#define NUM_MULSUB SIMD_MULSUB_F

#define NUM_ADDXX SIMD_ADDXX_F
#define NUM_SUBXX SIMD_SUBXX_F

#define NUM_ADDSQ SIMD_ADDSQ_F
#define NUM_SUBSQ SIMD_SUBSQ_F

#define NUM_NEG SIMD_NEG_F
#define NUM_ABS SIMD_ABS_F
#define NUM_SGN SIMD_SGN_F

#define NUM_SQRT SIMD_SQRT_F
#define NUM_POW SIMD_POW_F

#define NUM_SIN SIMD_SIN_F
#define NUM_COS SIMD_COS_F
#define NUM_ATAN2 SIMD_ATAN2_F
#define NUM_SINCOS SIMD_SINCOS_F

#elif defined(FORMULA_QD)
#include "qd/QDTypes.h"

typedef qd_number_t number_t;
typedef qd_num_2_t number_2_t;
typedef qd_param_t number_param_t;

#define NUM_CONST(x) qd_number_t(x)
#define NUM_VAR NUM_CONST

#define NUM_ADD(a, b) ((a) + (b))
#define NUM_SUB(a, b) ((a) - (b))
#define NUM_MUL(a, b) ((a) * (b))
#define NUM_DIV(a, b) ((a) / (b))

#define NUM_DOUBLE(x) ((x) * 2.0)
#define NUM_HALF(x) ((x) * 0.5)
#define NUM_SQUARE(x) ((x) * (x))

#define NUM_MULADD(a, b, c) ((a) * (b) + (c))
#define NUM_MULSUB(a, b, c) ((a) * (b) - (c))

#define NUM_ADDXX(a, b, c, d) ((a) * (b) + (c) * (d))
#define NUM_SUBXX(a, b, c, d) ((a) * (b) - (c) * (d))

#define NUM_ADDSQ(a, b) ((a) * (a) + (b) * (b))
#define NUM_SUBSQ(a, b) ((a) * (a) - (b) * (b))

#define NUM_NEG(x) -(x)
#define NUM_ABS(x) abs(x)
#define NUM_SGN(x) ((x) > 0.0 ? NUM_CONST(1.0) : ((x) < 0.0 ? NUM_CONST(-1.0) : NUM_CONST(0.0)))

#define NUM_SQRT(x) sqrt(x)
#define NUM_POW(a, b) pow(a, b)

#define NUM_SIN(x) sin(x)
#define NUM_COS(x) cos(x)
#define NUM_ATAN2(a, b) atan2(a, b)
#define NUM_SINCOS sincos_qd

#elif defined(FORMULA_MPFR)
#include "mpfr/MPFRTypes.h"

typedef mpfr_number_t number_t;
typedef mpfr_num_2_t number_2_t;
typedef mpfr_param_t number_param_t;

#define NUM_CONST(x) MPFRTypes::toNumber(static_cast<double>(x))
#define NUM_VAR NUM_CONST

#define NUM_ADD(a, b) MPFRTypes::add((a), (b))
#define NUM_SUB(a, b) MPFRTypes::sub((a), (b))
#define NUM_MUL(a, b) MPFRTypes::mul((a), (b))
#define NUM_DIV(a, b) MPFRTypes::div((a), (b))

#define NUM_DOUBLE(x) MPFRTypes::add((x), (x))
#define NUM_HALF(x) MPFRTypes::mul((x), NUM_CONST(0.5))
#define NUM_SQUARE(x) MPFRTypes::mul((x), (x))

#define NUM_MULADD(a, b, c) MPFRTypes::muladd((a), (b), (c))
#define NUM_MULSUB(a, b, c) MPFRTypes::mulsub((a), (b), (c))

#define NUM_ADDXX(a, b, c, d) MPFRTypes::addxx((a), (b), (c), (d))
#define NUM_SUBXX(a, b, c, d) MPFRTypes::subxx((a), (b), (c), (d))

#define NUM_ADDSQ(a, b) MPFRTypes::add( \
    MPFRTypes::mul((a), (a)), MPFRTypes::mul((b), (b)) \
)
#define NUM_SUBSQ(a, b) MPFRTypes::sub( \
    MPFRTypes::mul((a), (a)), MPFRTypes::mul((b), (b)) \
)

#define NUM_NEG(x) MPFRTypes::neg((x))
#define NUM_ABS(x) MPFRTypes::abs((x))
#define NUM_SGN(x) MPFRTypes::sgn((x))

#define NUM_SQRT(x) MPFRTypes::sqrt((x))
#define NUM_POW(a, b) MPFRTypes::pow((a), (b))

#define NUM_SIN(x) MPFRTypes::sin((x))
#define NUM_COS(x) MPFRTypes::cos((x))
#define NUM_ATAN2(a, b) MPFRTypes::atan2((a), (b))
#define NUM_SINCOS MPFRTypes::sincos(x)

#ifndef MPFR_FORMULA_SHARED_HELPERS
#define MPFR_FORMULA_SHARED_HELPERS

#include "mpfr/MPFRScratch.h"

FORCE_INLINE void setSign_mp(mpfr_t out, mpfr_srcptr in) {
    mpfr_set_si(out, mpfr_sgn(in), MPFRTypes::ROUNDING);
}

FORCE_INLINE void initFormulaConstants_mp(MPFRScratch &s) {
    mpfr_set_d(s.nVal, static_cast<double>(ScalarGlobals::N), MPFRTypes::ROUNDING);
    mpfr_sub_d(s.nMinus1, s.nVal, 1.0, MPFRTypes::ROUNDING);
    mpfr_mul_d(s.halfN, s.nVal, 0.5, MPFRTypes::ROUNDING);
    mpfr_mul_d(s.halfNMinus1, s.nMinus1, 0.5, MPFRTypes::ROUNDING);

    mpfr_set_d(s.lightR, static_cast<double>(ScalarGlobals::light_r), MPFRTypes::ROUNDING);
    mpfr_set_d(s.lightI, static_cast<double>(ScalarGlobals::light_i), MPFRTypes::ROUNDING);
}

FORCE_INLINE void wholePowerCore_mp(mpfr_srcptr pzr, mpfr_srcptr pzi,
    int N, MPFRScratch &s) {
    mpfr_set(s.t[2], pzr, MPFRTypes::ROUNDING);
    mpfr_set(s.t[3], pzi, MPFRTypes::ROUNDING);

    mpfr_set(s.t[4], pzr, MPFRTypes::ROUNDING);
    mpfr_set(s.t[5], pzi, MPFRTypes::ROUNDING);
    mpfr_mul(s.t[6], s.t[4], s.t[4], MPFRTypes::ROUNDING);
    mpfr_mul(s.t[7], s.t[5], s.t[5], MPFRTypes::ROUNDING);

    for (int k = N - 1; k > 0; k >>= 1) {
        if ((k & 1) == 1) {
            mpfr_mul(s.t[8], s.t[2], s.t[5], MPFRTypes::ROUNDING);

            mpfr_mul(s.t[9], s.t[4], s.t[2], MPFRTypes::ROUNDING);
            mpfr_mul(s.t[10], s.t[5], s.t[3], MPFRTypes::ROUNDING);
            mpfr_sub(s.t[9], s.t[9], s.t[10], MPFRTypes::ROUNDING);

            mpfr_mul(s.t[10], s.t[4], s.t[3], MPFRTypes::ROUNDING);
            mpfr_add(s.t[3], s.t[10], s.t[8], MPFRTypes::ROUNDING);
            mpfr_set(s.t[2], s.t[9], MPFRTypes::ROUNDING);
        }

        mpfr_mul(s.t[8], s.t[4], s.t[5], MPFRTypes::ROUNDING);
        mpfr_mul_2ui(s.t[5], s.t[8], 1, MPFRTypes::ROUNDING);
        mpfr_sub(s.t[4], s.t[6], s.t[7], MPFRTypes::ROUNDING);
        mpfr_mul(s.t[6], s.t[4], s.t[4], MPFRTypes::ROUNDING);
        mpfr_mul(s.t[7], s.t[5], s.t[5], MPFRTypes::ROUNDING);
    }

    mpfr_set(s.zr, s.t[2], MPFRTypes::ROUNDING);
    mpfr_set(s.zi, s.t[3], MPFRTypes::ROUNDING);
}

FORCE_INLINE void fractionalPowerCore_mp(mpfr_srcptr pzr, mpfr_srcptr pzi,
    mpfr_srcptr N, mpfr_srcptr halfN, MPFRScratch &s) {
    mpfr_pow(s.t[2], s.mag, halfN, MPFRTypes::ROUNDING);
    mpfr_atan2(s.t[3], pzi, pzr, MPFRTypes::ROUNDING);
    mpfr_mul(s.t[3], s.t[3], N, MPFRTypes::ROUNDING);

    mpfr_sin_cos(s.t[4], s.t[5], s.t[3], MPFRTypes::ROUNDING);

    mpfr_mul(s.zr, s.t[2], s.t[5], MPFRTypes::ROUNDING);
    mpfr_mul(s.zi, s.t[2], s.t[4], MPFRTypes::ROUNDING);
}

#endif

#else
#error "Must define a formula type."
#endif

#include "scalar/ScalarGlobals.h"
#define N_VAR NUM_VAR(ScalarGlobals::N)

constexpr int OP_CONST = 0;
constexpr int OP_VAR = 0;
constexpr int OP_ASSIGN = 0;

constexpr int OP_ADD = 1;
constexpr int OP_SUB = 1;
constexpr int OP_MUL = 1;
constexpr int OP_DIV = 1;

constexpr int OP_DOUBLE = 1;
constexpr int OP_HALF = 1;
constexpr int OP_SQUARE = 1;

#if _USE_FUSED_OPS

constexpr int OP_MULADD = 1;
constexpr int OP_MULSUB = 1;

constexpr int OP_ADDXX = 2;
constexpr int OP_SUBXX = 2;

constexpr int OP_ADDSQ = 2;
constexpr int OP_SUBSQ = 2;

constexpr int OP_SINCOS = 1;

#else

constexpr int OP_MULADD = 2;
constexpr int OP_MULSUB = 2;

constexpr int OP_ADDXX = 3;
constexpr int OP_SUBXX = 3;

constexpr int OP_ADDSQ = 3;
constexpr int OP_SUBSQ = 3;

constexpr int OP_SINCOS = 2;

#endif

constexpr int OP_NEG = 1;
constexpr int OP_ABS = 1;

constexpr int OP_SQRT = 1;
constexpr int OP_POW = 1;

constexpr int OP_SIN = 1;
constexpr int OP_COS = 1;
constexpr int OP_ATAN2 = 1;

#undef _USE_FUSED_OPS

#include "util/InlineUtil.h"

#ifdef _SKIP_FORMULA_OPS
#define _FORMULA_OPS_FUNC(body)
#define _DERIVATIVE_OPS_FUNC(body)
#else
#define _FORMULA_OPS_FUNC(body) \
    FORCE_INLINE int formulaOps() { \
        using namespace ScalarGlobals; \
        int count = 0; \
        body; \
        return count; \
    }

#define _DERIVATIVE_OPS_FUNC(body) \
    FORCE_INLINE int derivativeOps() { \
        int count = 0; \
        body; \
        return count; \
    }
#endif
