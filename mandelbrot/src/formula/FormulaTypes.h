#pragma once

#define _USE_FUSED_OPS 0

#if defined(FORMULA_SCALAR)
#include "../scalar/ScalarTypes.h"

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

#define NUM_SQRT SQRT_F
#define NUM_POW POW_F

#define NUM_SIN SIN_F
#define NUM_COS COS_F
#define NUM_ATAN2 ATAN2_F
#define NUM_SINCOS SINCOS_F

#elif defined(FORMULA_VECTOR)
#include "../vector/VectorTypes.h"

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

#define NUM_SQRT SIMD_SQRT_F
#define NUM_POW SIMD_POW_F

#define NUM_SIN SIMD_SIN_F
#define NUM_COS SIMD_COS_F
#define NUM_ATAN2 SIMD_ATAN2_F
#define NUM_SINCOS SIMD_SINCOS_F

#elif defined(FORMULA_MPFR)
#include "../mpfr/MpfrTypes.h"

typedef mpfr_number_t number_t;
typedef mpfr_num_2_t number_2_t;
typedef mpfr_param_t number_param_t;

#define NUM_CONST(x) mpfr_number_t(x)
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

#define NUM_SQRT(x) sqrt(x)
#define NUM_POW(a, b) pow(a, b)

#define NUM_SIN(x) sin(x)
#define NUM_COS(x) cos(x)
#define NUM_ATAN2(a, b) atan2(a, b)
#define NUM_SINCOS sincos_mp

#else
#error "Must define a formula type."
#endif

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