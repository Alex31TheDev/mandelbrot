#pragma once

#include "MacroUtil.h"

#if defined(_MSC_VER)
#define FORCE_INLINE __forceinline
#define NEVER_INLINE __declspec(noinline)

#define FULL_UNROLL _PRAGMA(loop(unroll(full)))
#define UNROLL(N) _PRAGMA(loop(unroll(N)))

#define VECTOR_CALL __vectorcall
#elif defined(__GNUC__) || defined(__clang__)
#define FORCE_INLINE inline __attribute__((always_inline))
#define NEVER_INLINE inline __attribute__((noinline))

#define FULL_UNROLL _PRAGMA(unroll)
#define UNROLL(N) _PRAGMA(unroll N)

#define VECTOR_CALL __attribute__((vectorcall))
#else
#define FORCE_INLINE inline
#define NEVER_INLINE

#define FULL_UNROLL
#define UNROLL(n)

#define VECTOR_CALL
#endif
