#include "ScalarCoords.h"

#include "ScalarTypes.h"

#include "ScalarGlobals.h"
using namespace ScalarGlobals;

scalar_full_t getCenterReal(int x) {
    return (x - halfWidth) * invWidth * realScale + point_r;
}

scalar_full_t getCenterImag(int y) {
    return (y - halfHeight) * invHeight * imagScale - point_i;
}