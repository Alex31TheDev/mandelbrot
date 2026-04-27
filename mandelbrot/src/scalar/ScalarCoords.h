#pragma once
#include "CommonDefs.h"

#include <algorithm>

#include "ScalarTypes.h"
#include "ScalarGlobals.h"

#include "util/InlineUtil.h"

FORCE_INLINE scalar_full_t getImageCenterReal(int x, scalar_full_t halfImgWidth,
    scalar_full_t invImgWidth) {
    using namespace ScalarGlobals;
    return (x - halfImgWidth) * invImgWidth * realScale + point_r;
}

FORCE_INLINE scalar_full_t getImageCenterImag(int y, scalar_full_t halfImgHeight,
    scalar_full_t invImgHeight) {
    using namespace ScalarGlobals;
    return (y - halfImgHeight) * invImgHeight * imagScale - point_i;
}

FORCE_INLINE scalar_full_t getCenterReal(int x) {
    using namespace ScalarGlobals;
    return getImageCenterReal(x, halfWidth, invWidth);
}

FORCE_INLINE scalar_full_t getCenterImag(int y) {
    using namespace ScalarGlobals;
    return getImageCenterImag(y, halfHeight, invHeight);
}

scalar_full_t getOutputCenterReal(int x);
scalar_full_t getOutputCenterImag(int y);

scalar_full_t getOutputPixelX(scalar_full_t real);
scalar_full_t getOutputPixelY(scalar_full_t imag);

void getOutputCenterPoint(int x, int y,
    scalar_full_t &real, scalar_full_t &imag);
void getOutputPixelPoint(scalar_full_t real, scalar_full_t imag,
    scalar_full_t &x, scalar_full_t &y);
void getPanCenterPoint(int deltaX, int deltaY,
    scalar_full_t &real, scalar_full_t &imag);
void getBoxCenterPoint(int left, int top, int right, int bottom,
    scalar_full_t &real, scalar_full_t &imag);
