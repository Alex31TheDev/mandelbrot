#pragma once
#include "CommonDefs.h"

#include <algorithm>

#include "ScalarTypes.h"
#include "ScalarGlobals.h"
#include "render/RenderGlobals.h"

#include "util/InlineUtil.h"

FORCE_INLINE int clampCoordToImage(int coord, int size) {
    return std::clamp(coord, 0, std::max(0, size - 1));
}

FORCE_INLINE scalar_full_t getCenterRealForImage(int x, scalar_full_t halfImgWidth,
    scalar_full_t invImgWidth) {
    using namespace ScalarGlobals;
    return (x - halfImgWidth) * invImgWidth * realScale + point_r;
}

FORCE_INLINE scalar_full_t getCenterImagForImage(int y, scalar_full_t halfImgHeight,
    scalar_full_t invImgHeight) {
    using namespace ScalarGlobals;
    return (y - halfImgHeight) * invImgHeight * imagScale - point_i;
}

FORCE_INLINE scalar_full_t getCenterReal(int x) {
    using namespace ScalarGlobals;
    return getCenterRealForImage(x, halfWidth, invWidth);
}

FORCE_INLINE scalar_full_t getCenterImag(int y) {
    using namespace ScalarGlobals;
    return getCenterImagForImage(y, halfHeight, invHeight);
}

FORCE_INLINE scalar_full_t getOutputCenterReal(int x) {
    using namespace RenderGlobals;
    const scalar_full_t halfOutputWidth = CAST_F(outputWidth) / SC_SYM_F(2.0);
    const scalar_full_t invOutputWidth = RECIP_F(outputWidth);

    return getCenterRealForImage(x, halfOutputWidth, invOutputWidth);
}

FORCE_INLINE scalar_full_t getOutputCenterImag(int y) {
    using namespace RenderGlobals;
    const scalar_full_t halfOutputHeight = CAST_F(outputHeight) / SC_SYM_F(2.0);
    const scalar_full_t invOutputHeight = RECIP_F(outputHeight);

    return getCenterImagForImage(y, halfOutputHeight, invOutputHeight);
}
