#include "ScalarCoords.h"

#include "../render/RenderGlobals.h"
#include "util/InlineUtil.h"

FORCE_INLINE int clampCoordToImage(int coord, int size) {
    return std::clamp(coord, 0, std::max(0, size - 1));
}

scalar_full_t getOutputCenterReal(int x) {
    using namespace RenderGlobals;
    const scalar_full_t halfOutputWidth = CAST_F(outputWidth) / SC_SYM_F(2.0),
        invOutputWidth = RECIP_F(outputWidth);

    return getImageCenterReal(x, halfOutputWidth, invOutputWidth);
}

scalar_full_t getOutputCenterImag(int y) {
    using namespace RenderGlobals;
    const scalar_full_t halfOutputHeight = CAST_F(outputHeight) / SC_SYM_F(2.0),
        invOutputHeight = RECIP_F(outputHeight);

    return getImageCenterImag(y, halfOutputHeight, invOutputHeight);
}

scalar_full_t getOutputPixelX(scalar_full_t real) {
    using namespace RenderGlobals;
    using namespace ScalarGlobals;

    const scalar_full_t halfOutputWidth = CAST_F(outputWidth) / SC_SYM_F(2.0);
    return ((real - point_r) / realScale) * CAST_F(outputWidth) + halfOutputWidth;
}

scalar_full_t getOutputPixelY(scalar_full_t imag) {
    using namespace RenderGlobals;
    using namespace ScalarGlobals;

    const scalar_full_t halfOutputHeight = CAST_F(outputHeight) / SC_SYM_F(2.0);
    return ((imag + point_i) / imagScale) * CAST_F(outputHeight) + halfOutputHeight;
}

void getOutputCenterPoint(int x, int y,
    scalar_full_t &real, scalar_full_t &imag) {
    real = getOutputCenterReal(x);
    imag = getOutputCenterImag(y);
}

void getOutputPixelPoint(scalar_full_t real, scalar_full_t imag,
    scalar_full_t &x, scalar_full_t &y) {
    x = getOutputPixelX(real);
    y = getOutputPixelY(imag);
}

void getPanCenterPoint(int deltaX, int deltaY,
    scalar_full_t &real, scalar_full_t &imag) {
    using namespace RenderGlobals;

    const int centerX = outputWidth / 2 - deltaX,
        centerY = outputHeight / 2 - deltaY;

    const int clampedX = clampCoordToImage(centerX, outputWidth),
        clampedY = clampCoordToImage(centerY, outputHeight);

    getOutputCenterPoint(clampedX, clampedY, real, imag);
    imag = -imag;
}

void getBoxCenterPoint(int left, int top, int right, int bottom,
    scalar_full_t &real, scalar_full_t &imag) {
    using namespace RenderGlobals;

    const int clampedLeft = clampCoordToImage(std::min(left, right), outputWidth);
    const int clampedRight = clampCoordToImage(std::max(left, right), outputWidth);
    const int clampedTop = clampCoordToImage(std::min(top, bottom), outputHeight);
    const int clampedBottom = clampCoordToImage(std::max(top, bottom), outputHeight);

    real = (getOutputCenterReal(clampedLeft) + getOutputCenterReal(clampedRight)) /
        SC_SYM_F(2.0);
    imag = -(getOutputCenterImag(clampedTop) + getOutputCenterImag(clampedBottom)) /
        SC_SYM_F(2.0);
}
