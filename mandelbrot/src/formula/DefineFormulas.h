#ifdef _FORMULA_TYPE

#if _FORMULA_TYPE == 0

#define _FORMULA_NAME normal

#define _FORMULA formula_normalPower
#define _FORMULA4_1 formula_normalPower4_1
#define _FORMULA4_2 formula_normalPower4_2
#define _DERIVATIVE derivative_normalPower
#define _DERIVATIVE4_1 derivative_normalPower4_1
#define _DERIVATIVE4_2 derivative_normalPower4_2

#elif _FORMULA_TYPE == 1

#define _FORMULA_NAME whole

#define _FORMULA formula_wholePower
#define _DERIVATIVE derivative_wholePower

#elif _FORMULA_TYPE == 2

#define _FORMULA_NAME fractional

#define _FORMULA formula_fractionalPower
#define _DERIVATIVE derivative_fractionalPower

#endif

#else

#endif
