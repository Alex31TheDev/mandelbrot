#include "../scalar/ScalarGlobals.h"

static inline double getCenterReal(int x) {
    using namespace ScalarGlobals;
    return (x - half_w) * scale + point_r;
}

static inline double getCenterImag(int y) {
    using namespace ScalarGlobals;
    return (y - half_h) * scale - point_i;
}