#pragma once

#include <cstdint>

#define IMG_STRIDE 3

extern int img_w, img_h;
extern uint8_t *pixels;

bool allocPixels(int width, int height);
bool saveImage(const char *filename);