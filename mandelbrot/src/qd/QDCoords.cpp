#ifdef USE_QD
#include "CommonDefs.h"
#include "QDCoords.h"

#include "render/RenderGlobals.h"
#include "util/InlineUtil.h"

FORCE_INLINE int clampCoordToImage_qd(int coord, int size) {
    return std::clamp(coord, 0, std::max(0, size - 1));
}

qd_number_t getOutputCenterReal_qd(int x) {
    using namespace QDGlobals;
    using namespace RenderGlobals;

    const int clampedX = clampCoordToImage_qd(x, outputWidth);
    const qd_number_t outputWidth_qd = static_cast<qd_number_t>(outputWidth);
    const qd_number_t x_qd = static_cast<qd_number_t>(clampedX);
    const qd_number_t halfOutputWidth = outputWidth_qd * qd_number_t(0.5);
    const qd_number_t invOutputWidth = qd_number_t(1.0) / outputWidth_qd;

    return (x_qd - halfOutputWidth) * invOutputWidth * realScale_qd + point_r_qd;
}

qd_number_t getOutputCenterImag_qd(int y) {
    using namespace QDGlobals;
    using namespace RenderGlobals;

    const int clampedY = clampCoordToImage_qd(y, outputHeight);
    const qd_number_t outputHeight_qd = static_cast<qd_number_t>(outputHeight);
    const qd_number_t y_qd = static_cast<qd_number_t>(clampedY);
    const qd_number_t halfOutputHeight = outputHeight_qd * qd_number_t(0.5);
    const qd_number_t invOutputHeight = qd_number_t(1.0) / outputHeight_qd;

    return (y_qd - halfOutputHeight) * invOutputHeight * imagScale_qd - point_i_qd;
}

qd_number_t getOutputPixelX_qd(qd_param_t real) {
    using namespace QDGlobals;
    using namespace RenderGlobals;

    const qd_number_t outputWidth_qd = static_cast<qd_number_t>(outputWidth);
    const qd_number_t halfOutputWidth = outputWidth_qd * qd_number_t(0.5);
    return ((real - point_r_qd) / realScale_qd) * outputWidth_qd + halfOutputWidth;
}

qd_number_t getOutputPixelY_qd(qd_param_t imag) {
    using namespace QDGlobals;
    using namespace RenderGlobals;

    const qd_number_t outputHeight_qd = static_cast<qd_number_t>(outputHeight);
    const qd_number_t halfOutputHeight = outputHeight_qd * qd_number_t(0.5);
    return ((imag + point_i_qd) / imagScale_qd) * outputHeight_qd + halfOutputHeight;
}

void getOutputCenterPoint_qd(
    qd_number_t &realOut, qd_number_t &imagOut,
    int x, int y
) {
    realOut = getOutputCenterReal_qd(x);
    imagOut = getOutputCenterImag_qd(y);
}

void getOutputPixelPoint_qd(
    qd_number_t &xOut, qd_number_t &yOut,
    qd_param_t real, qd_param_t imag
) {
    xOut = getOutputPixelX_qd(real);
    yOut = getOutputPixelY_qd(imag);
}

void getPanCenterPoint_qd(
    qd_number_t &realOut,
    qd_number_t &imagOut, int deltaX, int deltaY
) {
    using namespace RenderGlobals;

    const int centerX = outputWidth / 2 - deltaX;
    const int centerY = outputHeight / 2 - deltaY;
    const int clampedX = clampCoordToImage_qd(centerX, outputWidth);
    const int clampedY = clampCoordToImage_qd(centerY, outputHeight);

    getOutputCenterPoint_qd(realOut, imagOut, clampedX, clampedY);
    imagOut = -imagOut;
}

void getBoxCenterPoint_qd(
    qd_number_t &realOut,
    qd_number_t &imagOut, int left, int top, int right, int bottom
) {
    using namespace RenderGlobals;

    const int clampedLeft = clampCoordToImage_qd(std::min(left, right), outputWidth);
    const int clampedRight = clampCoordToImage_qd(std::max(left, right), outputWidth);
    const int clampedTop = clampCoordToImage_qd(std::min(top, bottom), outputHeight);
    const int clampedBottom = clampCoordToImage_qd(std::max(top, bottom), outputHeight);

    realOut = (getOutputCenterReal_qd(clampedLeft) +
        getOutputCenterReal_qd(clampedRight)) * qd_number_t(0.5);
    imagOut = (getOutputCenterImag_qd(clampedTop) +
        getOutputCenterImag_qd(clampedBottom)) * qd_number_t(-0.5);
}

#endif
