#include "RenderGlobals.h"

#include "../image/Image.h"

namespace RenderGlobals {
    int width = 0, height = 0, outputWidth = 0, outputHeight = 0, aaPixels = 1;
    bool useStreamIO = false, useThreads = false;

    bool setImageGlobals(int img_w, int img_h, int img_aaPixels) {
        if (img_w <= 0 || img_h <= 0 || img_aaPixels <= 0) return false;

        outputWidth = img_w;
        outputHeight = img_h;
        aaPixels = img_aaPixels;

        std::tie(width, height) = Image::calcRenderDimensions(
            outputWidth, outputHeight, Image::calcAAScale(aaPixels));

        return true;
    }
}