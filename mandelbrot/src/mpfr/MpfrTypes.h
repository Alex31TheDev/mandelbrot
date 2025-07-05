#pragma once

#include "mpreal.h"

#include "../util/InlineUtil.h"

typedef mpfr::mpreal mpfr_number_t;
typedef const mpfr::mpreal &mpfr_param_t;

struct mpfr_num_2_t {
    mpfr_number_t x, y;
};

FORCE_INLINE mpfr_num_2_t sincos_mp(mpfr_param_t x) {
    mpfr_num_2_t out;
    sincos(x, &out.x, &out.y);
    return out;
}
