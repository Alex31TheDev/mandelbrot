#pragma once
#ifdef USE_QD
#include "CommonDefs.h"

#include <algorithm>

#include "QDTypes.h"
#include "QDGlobals.h"
#include "../render/RenderGlobals.h"

#include "util/InlineUtil.h"

FORCE_INLINE qd_number_t getCenterReal_qd(int x) {
    using namespace QDGlobals;
    const qd_number_t x_qd = static_cast<qd_number_t>(x);
    return (x_qd - halfWidth_qd) * invWidth_qd * realScale_qd + point_r_qd;
}

FORCE_INLINE qd_number_t getCenterImag_qd(int y) {
    using namespace QDGlobals;
    const qd_number_t y_qd = static_cast<qd_number_t>(y);
    return (y_qd - halfHeight_qd) * invHeight_qd * imagScale_qd - point_i_qd;
}

FORCE_INLINE void getCenterReal_qd(qd_number_t &out, int x) {
    out = getCenterReal_qd(x);
}

FORCE_INLINE void getCenterImag_qd(qd_number_t &out, int y) {
    out = getCenterImag_qd(y);
}

FORCE_INLINE int clampCoordToImage_qd(int coord, int size) {
    return std::clamp(coord, 0, std::max(0, size - 1));
}

FORCE_INLINE qd_number_t getOutputCenterReal_qd(int x) {
    using namespace QDGlobals;
    using namespace RenderGlobals;

    const qd_number_t outputWidth_qd = static_cast<qd_number_t>(outputWidth);
    const qd_number_t x_qd = static_cast<qd_number_t>(x);
    const qd_number_t halfOutputWidth = outputWidth_qd * qd_number_t(0.5);
    const qd_number_t invOutputWidth = qd_number_t(1.0) / outputWidth_qd;

    return (x_qd - halfOutputWidth) * invOutputWidth * realScale_qd + point_r_qd;
}

FORCE_INLINE qd_number_t getOutputCenterImag_qd(int y) {
    using namespace QDGlobals;
    using namespace RenderGlobals;

    const qd_number_t outputHeight_qd = static_cast<qd_number_t>(outputHeight);
    const qd_number_t y_qd = static_cast<qd_number_t>(y);
    const qd_number_t halfOutputHeight = outputHeight_qd * qd_number_t(0.5);
    const qd_number_t invOutputHeight = qd_number_t(1.0) / outputHeight_qd;

    return (y_qd - halfOutputHeight) * invOutputHeight * imagScale_qd - point_i_qd;
}

FORCE_INLINE void getOutputCenterReal_qd(qd_number_t &out, int x) {
    out = getOutputCenterReal_qd(x);
}

FORCE_INLINE void getOutputCenterImag_qd(qd_number_t &out, int y) {
    out = getOutputCenterImag_qd(y);
}

#endif
