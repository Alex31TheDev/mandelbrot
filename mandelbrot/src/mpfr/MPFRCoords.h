#pragma once
#ifdef USE_MPFR
#include "CommonDefs.h"

#include <algorithm>

#include "MPFRTypes.h"
#include "MPFRGlobals.h"

#include "util/InlineUtil.h"

FORCE_INLINE int clampCoordToImage_mp(int coord, int size) {
    return std::clamp(coord, 0, std::max(0, size - 1));
}

FORCE_INLINE void getImageCenterReal_mp(mpfr_t out, int x,
    mpfr_srcptr halfImgWidth, mpfr_srcptr invImgWidth) {
    using namespace MPFRGlobals;

    mpfr_set_si(out, x, MPFRTypes::ROUNDING);
    mpfr_sub(out, out, halfImgWidth, MPFRTypes::ROUNDING);
    mpfr_mul(out, out, invImgWidth, MPFRTypes::ROUNDING);
    mpfr_mul(out, out, realScale_mp, MPFRTypes::ROUNDING);
    mpfr_add(out, out, point_r_mp, MPFRTypes::ROUNDING);
}

FORCE_INLINE void getImageCenterImag_mp(mpfr_t out, int y,
    mpfr_srcptr halfImgHeight, mpfr_srcptr invImgHeight) {
    using namespace MPFRGlobals;

    mpfr_set_si(out, y, MPFRTypes::ROUNDING);
    mpfr_sub(out, out, halfImgHeight, MPFRTypes::ROUNDING);
    mpfr_mul(out, out, invImgHeight, MPFRTypes::ROUNDING);
    mpfr_mul(out, out, imagScale_mp, MPFRTypes::ROUNDING);
    mpfr_sub(out, out, point_i_mp, MPFRTypes::ROUNDING);
}

FORCE_INLINE void getCenterReal_mp(mpfr_t out, int x) {
    using namespace MPFRGlobals;
    getImageCenterReal_mp(out, x, halfWidth_mp, invWidth_mp);
}

FORCE_INLINE void getCenterImag_mp(mpfr_t out, int y) {
    using namespace MPFRGlobals;
    getImageCenterImag_mp(out, y, halfHeight_mp, invHeight_mp);
}

void getOutputCenterReal_mp(mpfr_t out, int x);
void getOutputCenterImag_mp(mpfr_t out, int y);

void getOutputPixelX_mp(mpfr_t out, mpfr_srcptr real);
void getOutputPixelY_mp(mpfr_t out, mpfr_srcptr imag);

void getOutputCenterPoint_mp(mpfr_t realOut, mpfr_t imagOut,
    int x, int y);
void getOutputPixelPoint_mp(mpfr_t xOut, mpfr_t yOut,
    mpfr_srcptr real, mpfr_srcptr imag);
void getPanCenterPoint_mp(mpfr_t realOut, mpfr_t imagOut,
    int deltaX, int deltaY);
void getBoxCenterPoint_mp(mpfr_t realOut, mpfr_t imagOut,
    int left, int top, int right, int bottom);

#endif
