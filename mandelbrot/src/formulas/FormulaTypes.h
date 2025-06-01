#pragma once

#if defined(FORMULA_SCALAR)
#include "../scalar/ScalarTypes.h"

typedef scalar_full_t number_t;

#define NUM_CONST SC_SYM_F
#define NUM_VAR CAST_F

#define NUM_ADD(a, b) (CAST_F(a) + (b))
#define NUM_SUB(a, b) (CAST_F(a) - (b))
#define NUM_MUL(a, b) (CAST_F(a) * (b))
#define NUM_DIV(a, b) (CAST_F(a) / (b))

#define NUM_ABS ABS_F

#define NUM_SQRT SQRT_F
#define NUM_POW POW_F

#define NUM_SIN SIN_F
#define NUM_COS COS_F

#define NUM_ATAN2 ATAN2_F

#elif defined(FORMULA_VECTOR)
#include "../vector/VectorTypes.h"

typedef simd_full_t number_t;

#define NUM_CONST SIMD_SET_F
#define NUM_VAR SIMD_SET_F

#define NUM_ADD SIMD_ADD_F
#define NUM_SUB SIMD_SUB_F
#define NUM_MUL SIMD_MUL_F
#define NUM_DIV SIMD_DIV_F

#define NUM_ABS SIMD_ABS_F

#endif
