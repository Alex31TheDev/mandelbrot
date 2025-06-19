#pragma once

#if defined(_MSC_VER)
#define FORCE_INLINE __forceinline
#define NEVER_INLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define FORCE_INLINE inline __attribute__((always_inline))
#define NEVER_INLINE inline __attribute__((noinline))
#else
#define FORCE_INLINE inline
#define NEVER_INLINE
#endif