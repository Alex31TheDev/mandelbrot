#pragma once

#include "ScalarTypes.h"

#include <cstdint>

namespace ScalarRenderer {
    inline uint8_t pixelToInt(scalar_half_t val);
    inline void setPixel(uint8_t *pixels, int &pos,
        scalar_half_t R, scalar_half_t G, scalar_half_t B);
    void renderPixelScalar(uint8_t *pixels, int &pos,
        int x, scalar_full_t ci);
}
