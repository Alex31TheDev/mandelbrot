#pragma once

#include <cstdint>
#include <iostream>

bool writeBmpStream(std::ostream &fout, const uint8_t *pixels,
    int32_t width, int32_t height);
