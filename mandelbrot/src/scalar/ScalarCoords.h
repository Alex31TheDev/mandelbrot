#pragma once
#include "CommonDefs.h"

#include "ScalarTypes.h"
#include "ScalarGlobals.h"

#include "../util/InlineUtil.h"

FORCE_INLINE scalar_full_t getCenterReal(int x) {
    using namespace ScalarGlobals;
    return (x - halfWidth) * invWidth * realScale + point_r;
}

FORCE_INLINE scalar_full_t getCenterImag(int y) {
    using namespace ScalarGlobals;
    return (y - halfHeight) * invHeight * imagScale - point_i;
}