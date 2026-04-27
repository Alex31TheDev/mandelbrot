#pragma once
#ifdef USE_QD
#include "CommonDefs.h"

#include <algorithm>

#include "QDTypes.h"
#include "QDGlobals.h"

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

qd_number_t getOutputCenterReal_qd(int x);
qd_number_t getOutputCenterImag_qd(int y);

qd_number_t getOutputPixelX_qd(qd_param_t real);
qd_number_t getOutputPixelY_qd(qd_param_t imag);

void getOutputCenterPoint_qd(qd_number_t &realOut, qd_number_t &imagOut,
    int x, int y);
void getOutputPixelPoint_qd(qd_number_t &xOut, qd_number_t &yOut,
    qd_param_t real, qd_param_t imag);
void getPanCenterPoint_qd(qd_number_t &realOut,
    qd_number_t &imagOut, int deltaX, int deltaY);
void getBoxCenterPoint_qd(qd_number_t &realOut,
    qd_number_t &imagOut, int left, int top, int right, int bottom);

#endif
