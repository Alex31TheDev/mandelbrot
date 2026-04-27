#pragma once
#ifdef USE_QD
#include "CommonDefs.h"

#include <cstddef>
#include <cstdint>

#include "QDTypes.h"
#include "../render/RenderIterationStats.h"

#include "util/InlineUtil.h"

namespace QDRenderer {
    void initCoordsQD(qd_number_t &cr, qd_number_t &ci,
        qd_number_t &zr, qd_number_t &zi,
        qd_number_t &dr, qd_number_t &di);
    int iterateFractalQD(qd_param_t cr, qd_param_t ci,
        qd_number_t &zr, qd_number_t &zi,
        qd_number_t &dr, qd_number_t &di,
        qd_number_t &mag);

    void colorPixelQD(uint8_t *pixels, size_t &pos,
        int i, qd_param_t mag,
        qd_param_t zr, qd_param_t zi,
        qd_param_t dr, qd_param_t di);

    void renderPixelQD(uint8_t *pixels, size_t &pos,
        int x, qd_number_t ci,
        OptionalIterationStats iterStats = std::nullopt,
        std::optional<int> y = std::nullopt);
}

#endif
