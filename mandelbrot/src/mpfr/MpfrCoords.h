#pragma once
#ifdef USE_MPFR

#include "mpreal.h"

mpfr::mpreal getCenterReal_mp(int x);
mpfr::mpreal getCenterImag_mp(int y);

#endif
