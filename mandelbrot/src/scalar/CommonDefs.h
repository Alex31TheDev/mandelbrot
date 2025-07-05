#pragma once

#if defined(USE_SCALAR) || defined(USE_MPFR)
#define USE_SCALAR_COLORING 1
#else
#define USE_SCALAR_COLORING 0
#endif
