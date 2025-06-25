#pragma once
#include "CommonDefs.h"

#include <cstdlib>
#include <cstdint>
#include <cmath>

#include "../util/MacroUtil.h"
#include "../util/InlineUtil.h"

#if defined(USE_FLOATS)
typedef float scalar_full_t;
#define SC_SYM_F(a) _CONCAT2(a, f)
#define SC_SYMS_F SC_SYM_F
#elif defined(USE_DOUBLES)
typedef double scalar_full_t;
#define SC_SYM_F(a) _EVAL(a)
#define SC_SYMS_F(a) _CONCAT2(a, d)
#else
#error "Must define either USE_FLOATS or USE_DOUBLES to select precision."
#endif

typedef float scalar_half_t;
#define SC_SYM_H(a) _CONCAT2(a, f)
#define SC_SYMS_H SC_SYM_H

struct scalar_full_2_t {
    scalar_full_t x, y;
};
struct scalar_half_2_t {
    scalar_half_t x, y;
};

#define CAST_F(x) static_cast<scalar_full_t>(x)
#define CAST_H(x) static_cast<scalar_half_t>(x)

#define CAST_INT_S(x, size) _CONCAT3(static_cast<int, size, _t>)(x)
#define CAST_INT_U(x, size) _CONCAT3(static_cast<uint, size, _t>)(x)

#define RECIP_F(x) (1 / CAST_F(x))
#define RECIP_H(x) (1 / CAST_H(x))

#define ZERO_F SC_SYM_F(0.0)
#define ZERO_H SC_SYM_H(0.0)

#define ONE_F SC_SYM_F(1.0)
#define ONE_H SC_SYM_H(1.0)

#define HALF_F SC_SYM_F(0.5)
#define HALF_H SC_SYM_H(0.5)

#define NEG_ONE_F SC_SYM_F(-1.0)
#define NEG_ONE_H SC_SYM_H(-1.0)

#define IS0_F(x) (CAST_F(x) == ZERO_F)
#define IS0_H(x) (CAST_H(x) == ZERO_H)

#define ISNOT0_F(x) (CAST_F(x) != ZERO_F)
#define ISNOT0_H(x) (CAST_H(x) != ZERO_H)

#define ISPOS_F(x) (CAST_F(x) > ZERO_F)
#define ISPOS_H(x) (CAST_H(x) > ZERO_H)

#define ISPOS0_F(x) (CAST_F(x) >= ZERO_F)
#define ISPOS0_H(x) (CAST_H(x) >= ZERO_H)

#define ISNEG_F(x) (CAST_F(x) < ZERO_F)
#define ISNEG_H(x) (CAST_H(x) < ZERO_H)

#define ISNEG0_F(x) (CAST_F(x) <= ZERO_F)
#define ISNEG0_H(x) (CAST_H(x) <= ZERO_H)

#define SC_FUNC1_F(name, x) SC_SYM_F(name)(CAST_F(x))
#define SC_FUNC1_H(name, x) SC_SYM_H(name)(CAST_H(x))

#define FUNCS_F(name, ...) SC_SYMS_F(name)(__VA_ARGS__)
#define FUNCS_H(name, ...) SC_SYMS_H(name)(__VA_ARGS__)

#define SC_FUNC2_F(name, a, b) SC_SYM_F(name)(CAST_F(a), CAST_F(b))
#define SC_FUNC2_H(name, a, b) SC_SYM_H(name)(CAST_H(a), CAST_H(b))

#define PARSE_F(str) FUNCS_F(strto, str, nullptr)
#define PARSE_H(str) FUNCS_H(strto, str, nullptr)

#define PARSE_INT32(str) static_cast<int32_t>(strtol(str, nullptr, 10))
#define PARSE_INT64(str) static_cast<int64_t>(strtoll(str, nullptr, 10))

#define ABS_F(x) SC_FUNC1_F(fabs, x)
#define ABS_H(x) SC_FUNC1_H(fabs, x)

#define FLOOR_F(x) SC_FUNC1_F(floor, x)
#define FLOOR_H(x) SC_FUNC1_H(floor, x)

#define CEIL_F(x) SC_FUNC1_F(ceil, x)
#define CEIL_H(x) SC_FUNC1_H(ceil, x)

#define FRAC_F(x) (CAST_F(x) - FLOOR_F(x))
#define FRAC_H(x) (CAST_H(x) - FLOOR_H(x))

#define ISWHOLE_F(x) (CAST_F(x) == FLOOR_F(x))
#define ISWHOLE_H(x) (CAST_H(x) == FLOOR_H(x))

#define MIN_F(a, b) SC_FUNC2_F(fmin, a, b)
#define MIN_H(a, b) SC_FUNC2_H(fmin, a, b)

#define MAX_F(a, b) SC_FUNC2_F(fmax, a, b)
#define MAX_H(a, b) SC_FUNC2_H(fmax, a, b)

#define CLAMP_F(x, a, b) MIN_F(MAX_F(x, a), b)
#define CLAMP_H(x, a, b) MIN_H(MAX_H(x, a), b)

#define SQRT_F(x) SC_FUNC1_F(sqrt, x)
#define SQRT_H(x) SC_FUNC1_H(sqrt, x)

#define POW_F(a, b) SC_FUNC2_F(pow, a, b)
#define POW_H(a, b) SC_FUNC2_H(pow, a, b)

#define LOG_F(x) SC_FUNC1_F(log, x)
#define LOG_H(x) SC_FUNC1_H(log, x)

#define LOG10_F(x) SC_FUNC1_F(log10, x)
#define LOG10_H(x) SC_FUNC1_H(log10, x)

#define SIN_F(x) SC_FUNC1_F(sin, x)
#define SIN_H(x) SC_FUNC1_H(sin, x)

#define COS_F(x) SC_FUNC1_F(cos, x)
#define COS_H(x) SC_FUNC1_H(cos, x)

#define ATAN2_F(a, b) SC_FUNC2_F(atan2, a, b)
#define ATAN2_H(a, b) SC_FUNC2_H(atan2, a, b)

FORCE_INLINE scalar_full_2_t SINCOS_F(scalar_full_t x) {
    scalar_full_2_t out{};

#if defined(__GNUC__) || defined(__clang__)
    SC_SYM_F(sincos)(CAST_F(x), &out.x, &out.y);
#else
    out.x = SIN_F(x);
    out.y = COS_F(x);
#endif

    return out;
}
FORCE_INLINE scalar_half_2_t SINCOS_H(scalar_half_t x) {
    scalar_half_2_t out{};

#if defined(__GNUC__) || defined(__clang__)
    SC_SYM_H(sincos)(CAST_H(x), &out.x, &out.y);
#else
    out.x = SIN_H(x);
    out.y = COS_H(x);
#endif

    return out;
}

#define NEXTAFTER_F(a, b) SC_FUNC2_F(nextafter, a, b)
#define NEXTAFTER_H(a, b) SC_FUNC2_H(nextafter, a, b)
