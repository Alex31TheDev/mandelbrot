#ifdef USE_MPFR
#include "CommonDefs.h"
#include "MPFRCoords.h"

#include "render/RenderGlobals.h"

void getOutputCenterReal_mp(mpfr_t out, int x) {
    using namespace RenderGlobals;
    const int clampedX = clampCoordToImage_mp(x, outputWidth);

    mpfr_ptr halfOutputWidth = MPFRTypes::nextTemp();
    mpfr_set_si(halfOutputWidth, outputWidth, MPFRTypes::ROUNDING);
    mpfr_div_2ui(halfOutputWidth, halfOutputWidth, 1, MPFRTypes::ROUNDING);

    mpfr_ptr invOutputWidth = MPFRTypes::nextTemp();
    mpfr_set_si(invOutputWidth, outputWidth, MPFRTypes::ROUNDING);
    mpfr_ui_div(invOutputWidth, 1, invOutputWidth, MPFRTypes::ROUNDING);

    getImageCenterReal_mp(out, clampedX, halfOutputWidth, invOutputWidth);
}

void getOutputCenterImag_mp(mpfr_t out, int y) {
    using namespace RenderGlobals;
    const int clampedY = clampCoordToImage_mp(y, outputHeight);

    mpfr_ptr halfOutputHeight = MPFRTypes::nextTemp();
    mpfr_set_si(halfOutputHeight, outputHeight, MPFRTypes::ROUNDING);
    mpfr_div_2ui(halfOutputHeight, halfOutputHeight, 1, MPFRTypes::ROUNDING);

    mpfr_ptr invOutputHeight = MPFRTypes::nextTemp();
    mpfr_set_si(invOutputHeight, outputHeight, MPFRTypes::ROUNDING);
    mpfr_ui_div(invOutputHeight, 1, invOutputHeight, MPFRTypes::ROUNDING);

    getImageCenterImag_mp(out, clampedY, halfOutputHeight, invOutputHeight);
}

void getOutputPixelX_mp(mpfr_t out, mpfr_srcptr real) {
    using namespace RenderGlobals;
    using namespace MPFRGlobals;

    mpfr_t widthMp;
    mpfr_t halfOutputWidth;
    mpfr_inits2(MPFRTypes::precisionBits,
        widthMp, halfOutputWidth, static_cast<mpfr_ptr>(nullptr));

    mpfr_set_si(widthMp, outputWidth, MPFRTypes::ROUNDING);
    mpfr_div_2ui(halfOutputWidth, widthMp, 1, MPFRTypes::ROUNDING);

    mpfr_sub(out, real, point_r_mp, MPFRTypes::ROUNDING);
    mpfr_div(out, out, realScale_mp, MPFRTypes::ROUNDING);
    mpfr_mul(out, out, widthMp, MPFRTypes::ROUNDING);
    mpfr_add(out, out, halfOutputWidth, MPFRTypes::ROUNDING);

    mpfr_clears(widthMp, halfOutputWidth, static_cast<mpfr_ptr>(nullptr));
}

void getOutputPixelY_mp(mpfr_t out, mpfr_srcptr imag) {
    using namespace RenderGlobals;
    using namespace MPFRGlobals;

    mpfr_t heightMp;
    mpfr_t halfOutputHeight;
    mpfr_inits2(MPFRTypes::precisionBits,
        heightMp, halfOutputHeight, static_cast<mpfr_ptr>(nullptr));

    mpfr_set_si(heightMp, outputHeight, MPFRTypes::ROUNDING);
    mpfr_div_2ui(halfOutputHeight, heightMp, 1, MPFRTypes::ROUNDING);

    mpfr_add(out, imag, point_i_mp, MPFRTypes::ROUNDING);
    mpfr_div(out, out, imagScale_mp, MPFRTypes::ROUNDING);
    mpfr_mul(out, out, heightMp, MPFRTypes::ROUNDING);
    mpfr_add(out, out, halfOutputHeight, MPFRTypes::ROUNDING);

    mpfr_clears(heightMp, halfOutputHeight, static_cast<mpfr_ptr>(nullptr));
}

void getOutputCenterPoint_mp(
    mpfr_t realOut, mpfr_t imagOut,
    int x, int y
) {
    getOutputCenterReal_mp(realOut, x);
    getOutputCenterImag_mp(imagOut, y);
}

void getOutputPixelPoint_mp(
    mpfr_t xOut, mpfr_t yOut,
    mpfr_srcptr real, mpfr_srcptr imag
) {
    getOutputPixelX_mp(xOut, real);
    getOutputPixelY_mp(yOut, imag);
}

void getPanCenterPoint_mp(
    mpfr_t realOut, mpfr_t imagOut,
    int deltaX, int deltaY
) {
    using namespace RenderGlobals;

    const int centerX = outputWidth / 2 - deltaX;
    const int centerY = outputHeight / 2 - deltaY;
    const int clampedX = clampCoordToImage_mp(centerX, outputWidth);
    const int clampedY = clampCoordToImage_mp(centerY, outputHeight);

    getOutputCenterPoint_mp(realOut, imagOut, clampedX, clampedY);
    mpfr_neg(imagOut, imagOut, MPFRTypes::ROUNDING);
}

void getBoxCenterPoint_mp(
    mpfr_t realOut, mpfr_t imagOut,
    int left, int top, int right, int bottom
) {
    using namespace RenderGlobals;

    const int clampedLeft = clampCoordToImage_mp(std::min(left, right), outputWidth);
    const int clampedRight = clampCoordToImage_mp(std::max(left, right), outputWidth);
    const int clampedTop = clampCoordToImage_mp(std::min(top, bottom), outputHeight);
    const int clampedBottom = clampCoordToImage_mp(std::max(top, bottom), outputHeight);

    mpfr_t leftRealMp, rightRealMp, topImagMp, bottomImagMp;
    mpfr_inits2(MPFRTypes::precisionBits,
        leftRealMp, rightRealMp, topImagMp, bottomImagMp,
        static_cast<mpfr_ptr>(nullptr));

    getOutputCenterReal_mp(leftRealMp, clampedLeft);
    getOutputCenterReal_mp(rightRealMp, clampedRight);
    getOutputCenterImag_mp(topImagMp, clampedTop);
    getOutputCenterImag_mp(bottomImagMp, clampedBottom);

    mpfr_add(realOut, leftRealMp, rightRealMp, MPFRTypes::ROUNDING);
    mpfr_div_2ui(realOut, realOut, 1, MPFRTypes::ROUNDING);
    mpfr_add(imagOut, topImagMp, bottomImagMp, MPFRTypes::ROUNDING);
    mpfr_div_2ui(imagOut, imagOut, 1, MPFRTypes::ROUNDING);
    mpfr_neg(imagOut, imagOut, MPFRTypes::ROUNDING);

    mpfr_clears(leftRealMp, rightRealMp, topImagMp, bottomImagMp,
        static_cast<mpfr_ptr>(nullptr));
}

#endif