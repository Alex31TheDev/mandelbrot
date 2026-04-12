#ifdef _FORMULA_TYPE

#if _FORMULA_TYPE == 0

#define _FORMULA_NAME normal

#define _FORMULA formula_normalPower
#define _DERIVATIVE derivative_normalPower

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