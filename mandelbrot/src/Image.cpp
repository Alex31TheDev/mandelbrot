#include <cstdlib>
#include <cstdio>
#include <cstdint>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "Image.h"

int img_w, img_h;
uint8_t *pixels = NULL;

bool allocPixels(int width, int height) {
    img_w = width;
    img_h = height;

    pixels = new uint8_t[img_w * img_h * IMG_STRIDE];

    if (!pixels) {
        fprintf(stderr, "Out of memory.\n");
        return false;
    }

    return true;
}

bool saveImage(const char *filename) {
    if (!stbi_write_png(filename, img_w, img_h, 3, pixels, img_w * IMG_STRIDE)) {
        delete pixels;
        pixels = NULL;

        fprintf(stderr, "Failed to write PNG.\n");
        return false;
    }

    delete pixels;
    pixels = NULL;

    char absPath[_MAX_PATH];
    _fullpath(absPath, filename, _MAX_PATH);
    printf("Wrote: %s (%dx%d)\n", absPath, img_w, img_h);

    return true;
}