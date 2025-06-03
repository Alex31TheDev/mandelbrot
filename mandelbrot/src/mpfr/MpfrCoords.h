#pragma once
#ifdef USE_MPFR

#include "mpreal.h"

const mpfr::mpreal getCenterReal_mp(const int x);
const mpfr::mpreal getCenterImag_mp(const int y);

#endif
