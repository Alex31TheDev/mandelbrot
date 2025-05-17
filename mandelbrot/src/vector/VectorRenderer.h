#pragma once

#include <cstdint>
#include <xmmintrin.h>

namespace VectorRenderer {
    inline void setPixels_vec(uint8_t *pixels, int &pos, int width, __m128 R, __m128 G, __m128 B);
    void renderPixelSimd(uint8_t *pixels, int &pos, int width, int x, double ci);
}
