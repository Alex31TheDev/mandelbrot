#pragma once

#if defined(FORMULA_SCALAR)
#include "../scalar/ScalarTypes.h"

typedef scalar_full_t number_t;
typedef number_t number_param_t;

#define NUM_CONST SC_SYM_F
#define NUM_VAR CAST_F

#define NUM_ADD(a, b) (CAST_F(a) + (b))
#define NUM_SUB(a, b) (CAST_F(a) - (b))
#define NUM_MUL(a, b) (CAST_F(a) * (b))
#define NUM_DIV(a, b) (CAST_F(a) / (b))

#define NUM_NEG(x) -(x)
#define NUM_ABS ABS_F

#define NUM_SQRT SQRT_F
#define NUM_POW POW_F

#define NUM_SIN SIN_F
#define NUM_COS COS_F
#define NUM_ATAN2 ATAN2_F

#elif defined(FORMULA_VECTOR)
#include "../vector/VectorTypes.h"

typedef simd_full_t number_t;
typedef const number_t &number_param_t;

#define NUM_CONST SIMD_SET_F
#define NUM_VAR NUM_CONST

#define NUM_ADD SIMD_ADD_F
#define NUM_SUB SIMD_SUB_F
#define NUM_MUL SIMD_MUL_F
#define NUM_DIV SIMD_DIV_F

#define NUM_NEG SIMD_NEG_F
#define NUM_ABS SIMD_ABS_F

#define NUM_SQRT SIMD_SQRT_F
#define NUM_POW SIMD_POW_F

#define NUM_SIN SIMD_SIN_F
#define NUM_COS SIMD_COS_F
#define NUM_ATAN2 SIMD_ATAN2_F

#elif defined(FORMULA_MPFR)
#include "../mpfr/mpreal.h"

typedef mpfr::mpreal number_t;
typedef const number_t &number_param_t;

#define NUM_CONST(x) mpfr::mpreal(x)
#define NUM_VAR NUM_CONST

#define NUM_ADD(a, b) ((a) + (b))
#define NUM_SUB(a, b) ((a) - (b))
#define NUM_MUL(a, b) ((a) * (b))
#define NUM_DIV(a, b) ((a) / (b))

#define NUM_NEG(x) -(x)
#define NUM_ABS(x) abs(x)

#define NUM_SQRT(x) sqrt(x)
#define NUM_POW(a, b) pow(a, b)

#define NUM_SIN(x) sin(x)
#define NUM_COS(x) cos(x)
#define NUM_ATAN2(a, b) atan2(a, b)

#else
#error "Must define a formula type."
#endif
