#pragma once

#include <cstdint>

namespace ScalarRenderer {
    inline void setPixel(uint8_t *pixels, int &pos,
        float R, float G, float B);
    void renderPixelScalar(uint8_t *pixels, int &pos,
        int x, double ci);
}
