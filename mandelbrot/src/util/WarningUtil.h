#pragma once

#if defined(_MSC_VER)
#define PUSH_DISABLE_WARNINGS __pragma(warning(push, 0))
#define POP_DISABLE_WARNINGS __pragma(warning(pop))
#elif defined(__clang__) || defined(__GNUC__)
#define _PRAGMA(P) _Pragma(#P)

#define _GCC_PUSH_DISABLE_WARNINGS _PRAGMA(GCC diagnostic push)
#define _DISABLE_GCC_WARNING(WARNING) _PRAGMA(GCC diagnostic ignored #WARNING)

#if __clang__
#define PUSH_DISABLE_WARNINGS _GCC_PUSH_DISABLE_WARNINGS \
    _DISABLE_GCC_WARNING(-Weffc++) \
    _DISABLE_GCC_WARNING(-Wall) \
    _DISABLE_GCC_WARNING(-Wconversion) \
    _DISABLE_GCC_WARNING(-Wextra) \
    _DISABLE_GCC_WARNING(-Wattributes) \
    _DISABLE_GCC_WARNING(-Wimplicit-fallthrough) \
    _DISABLE_GCC_WARNING(-Wnon-virtual-dtor) \
    _DISABLE_GCC_WARNING(-Wreturn-type) \
    _DISABLE_GCC_WARNING(-Wshadow) \
    _DISABLE_GCC_WARNING(-Wunused-parameter) \
    _DISABLE_GCC_WARNING(-Wunused-variable)
#else
#define PUSH_DISABLE_WARNINGS _GCC_PUSH_DISABLE_WARNINGS \
    _DISABLE_GCC_WARNING(-Weffc++) \
    _DISABLE_GCC_WARNING(-Wall) \
    _DISABLE_GCC_WARNING(-Wconversion) \
    _DISABLE_GCC_WARNING(-Wextra) \
    _DISABLE_GCC_WARNING(-Wattributes) \
    _DISABLE_GCC_WARNING(-Wimplicit-fallthrough) \
    _DISABLE_GCC_WARNING(-Wnon-virtual-dtor) \
    _DISABLE_GCC_WARNING(-Wreturn-type) \
    _DISABLE_GCC_WARNING(-Wshadow) \
    _DISABLE_GCC_WARNING(-Wunused-parameter) \
    _DISABLE_GCC_WARNING(-Wunused-variable) \
    _DISABLE_GCC_WARNING(-Wmaybe-uninitialized) \
    _DISABLE_GCC_WARNING(-Wformat-security)
#endif

#define POP_DISABLE_WARNINGS _PRAGMA(GCC diagnostic pop)
#else
#define PUSH_DISABLE_WARNINGS
#define POP_DISABLE_WARNINGS
#endif