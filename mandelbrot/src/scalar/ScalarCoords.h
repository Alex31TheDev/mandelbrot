#pragma once

#include "ScalarTypes.h"
#include "ScalarGlobals.h"

static inline scalar_full_t getCenterReal(int x) {
    using namespace ScalarGlobals;
    return ((x - halfWidth) * invWidth) * scale + point_r;
}

static inline scalar_full_t getCenterImag(int y) {
    using namespace ScalarGlobals;
    return ((halfHeight - y) * invHeight) * (scale / aspect) + point_i;
}