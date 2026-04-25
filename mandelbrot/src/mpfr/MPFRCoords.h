#pragma once
#ifdef USE_MPFR
#include "CommonDefs.h"

#include <algorithm>

#include "MPFRTypes.h"
#include "MPFRGlobals.h"
#include "../render/RenderGlobals.h"

#include "util/InlineUtil.h"

FORCE_INLINE int clampCoordToImage_mp(int coord, int size) {
    return std::clamp(coord, 0, std::max(0, size - 1));
}

FORCE_INLINE void getCenterRealForImage_mp(mpfr_t out, int x,
    mpfr_srcptr halfImgWidth, mpfr_srcptr invImgWidth) {
    using namespace MPFRGlobals;

    mpfr_set_si(out, x, MPFRTypes::ROUNDING);
    mpfr_sub(out, out, halfImgWidth, MPFRTypes::ROUNDING);
    mpfr_mul(out, out, invImgWidth, MPFRTypes::ROUNDING);
    mpfr_mul(out, out, realScale_mp, MPFRTypes::ROUNDING);
    mpfr_add(out, out, point_r_mp, MPFRTypes::ROUNDING);
}

FORCE_INLINE void getCenterImagForImage_mp(mpfr_t out, int y,
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

    getCenterRealForImage_mp(out, x, halfWidth_mp, invWidth_mp);
}

FORCE_INLINE void getCenterImag_mp(mpfr_t out, int y) {
    using namespace MPFRGlobals;

    getCenterImagForImage_mp(out, y, halfHeight_mp, invHeight_mp);
}

FORCE_INLINE void getOutputCenterReal_mp(mpfr_t out, int x) {
    using namespace RenderGlobals;

    mpfr_ptr halfOutputWidth = MPFRTypes::nextTemp();
    mpfr_set_si(halfOutputWidth, outputWidth, MPFRTypes::ROUNDING);
    mpfr_div_2ui(halfOutputWidth, halfOutputWidth, 1, MPFRTypes::ROUNDING);

    mpfr_ptr invOutputWidth = MPFRTypes::nextTemp();
    mpfr_set_si(invOutputWidth, outputWidth, MPFRTypes::ROUNDING);
    mpfr_ui_div(invOutputWidth, 1, invOutputWidth, MPFRTypes::ROUNDING);

    getCenterRealForImage_mp(out, x, halfOutputWidth, invOutputWidth);
}

FORCE_INLINE void getOutputCenterImag_mp(mpfr_t out, int y) {
    using namespace RenderGlobals;

    mpfr_ptr halfOutputHeight = MPFRTypes::nextTemp();
    mpfr_set_si(halfOutputHeight, outputHeight, MPFRTypes::ROUNDING);
    mpfr_div_2ui(halfOutputHeight, halfOutputHeight, 1, MPFRTypes::ROUNDING);

    mpfr_ptr invOutputHeight = MPFRTypes::nextTemp();
    mpfr_set_si(invOutputHeight, outputHeight, MPFRTypes::ROUNDING);
    mpfr_ui_div(invOutputHeight, 1, invOutputHeight, MPFRTypes::ROUNDING);

    getCenterImagForImage_mp(out, y, halfOutputHeight, invOutputHeight);
}

#endif
