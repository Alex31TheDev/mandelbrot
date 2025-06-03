#include "ScalarCoords.h"

#include "ScalarTypes.h"

#include "ScalarGlobals.h"
using namespace ScalarGlobals;

const scalar_full_t getCenterReal(const int x) {
    return (x - halfWidth) * invWidth * realScale + point_r;
}

const scalar_full_t getCenterImag(const int y) {
    return (y - halfHeight) * invHeight * imagScale - point_i;
}