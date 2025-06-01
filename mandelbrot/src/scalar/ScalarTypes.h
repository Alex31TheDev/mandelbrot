#pragma once

#include <cstdlib>
#include <cstdint>

#include "../util/MacroUtil.h"

#ifdef USE_DOUBLES
typedef double scalar_full_t;
#define SC_SYM_F(a) _EVAL(a)
#define SC_SYMS_F(a) _CONCAT2(a, d)
#else
typedef float scalar_full_t;
#define SC_SYM_F(a) _CONCAT2(a, f)
#define SC_SYMS_F SC_SYM_F
#endif

typedef float scalar_half_t;
#define SC_SYM_H(a) _CONCAT2(a, f)
#define SC_SYMS_H SC_SYM_H

#define CAST_F(x) static_cast<scalar_full_t>(x)
#define CAST_H(x) static_cast<scalar_half_t>(x)

#define CAST_INT_S(x, size) _CONCAT3(static_cast<int, size, _t>)(x)
#define CAST_INT_U(x, size) _CONCAT3(static_cast<uint, size, _t>)(x)

#define RECIP_F(x) (1 / CAST_F(x))
#define RECIP_H(x) (1 / CAST_H(x))

#define FUNC1_F(name, x) SC_SYM_F(name)(CAST_F(x))
#define FUNC1_H(name, x) SC_SYM_H(name)(CAST_H(x))

#define FUNCS_F(name, ...) SC_SYMS_F(name)(__VA_ARGS__)
#define FUNCS_H(name, ...) SC_SYMS_H(name)(__VA_ARGS__)

#define FUNC2_F(name, a, b) SC_SYM_F(name)(CAST_F(a), CAST_F(b))
#define FUNC2_H(name, a, b) SC_SYM_H(name)(CAST_H(a), CAST_H(b))

#define ABS_F(x) FUNC1_F(fabs, x)
#define ABS_H(x) FUNC1_H(fabs, x)

#define LOG_F(x) FUNC1_F(log, x)
#define LOG_H(x) FUNC1_H(log, x)

#define LOG10_H(x) FUNC1_H(log10, x)

#define POW_F(a, b) FUNC2_F(pow, a, b)
#define POW_H(a, b) FUNC2_H(pow, a, b)

#define SQRT_F(x) FUNC1_F(sqrt, x)
#define SQRT_H(x) FUNC1_H(sqrt, x)

#define SIN_F(x) FUNC1_F(sin, x)
#define SIN_H(x) FUNC1_H(sin, x)

#define COS_F(x) FUNC1_F(cos, x)
#define COS_H(x) FUNC1_H(cos, x)

#define ATAN2_F(a, b) FUNC2_F(atan2, a, b)

#define MIN_H(a, b) FUNC2_H(fmin, a, b)
#define MAX_H(a, b) FUNC2_H(fmax, a, b)

#define PARSE_F(str) FUNCS_F(strto, str, nullptr)
#define PARSE_H(str) FUNCS_H(strto, str, nullptr)

#define PARSE_INT32(str) static_cast<int32_t>(strtol(str, nullptr, 10))
#define PARSE_INT64(str) static_cast<int64_t>(strtoll(str, nullptr, 10))
