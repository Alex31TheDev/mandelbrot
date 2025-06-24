#pragma once

#if defined(_MSC_VER)
#define FORCE_INLINE __forceinline
#define NEVER_INLINE __declspec(noinline)

#define VECTOR_CALL __vectorcall
#elif defined(__GNUC__) || defined(__clang__)
#define FORCE_INLINE inline __attribute__((always_inline))
#define NEVER_INLINE inline __attribute__((noinline))

#define VECTOR_CALL __attribute__((vectorcall))
#else
#define FORCE_INLINE inline
#define NEVER_INLINE

#define VECTOR_CALL
#endif
