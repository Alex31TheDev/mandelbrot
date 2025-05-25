#pragma once

#include "ScalarGlobals.h"

static inline double getCenterReal(int x) {
    using namespace ScalarGlobals;
    return ((x - halfWidth) * invWidth) * scale + point_r;
}

static inline double getCenterImag(int y) {
    using namespace ScalarGlobals;
    return ((halfHeight - y) * invHeight) * (scale / aspect) + point_i;
}