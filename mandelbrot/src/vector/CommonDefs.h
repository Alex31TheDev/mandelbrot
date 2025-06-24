#pragma once

#if defined(__SSE4_2__)
#define SSE 1
#else
#define SSE 0
#endif
#if defined(__AVX2__)
#define AVX2 1
#else
#define AVX2 0
#endif
#if defined(__AVX512__)
#define AVX512 1
#else
#define AVX512 0
#endif

#ifndef _MSC_VER
#define USE_SLEEF
#endif

#define USE_VECTOR_STORE
