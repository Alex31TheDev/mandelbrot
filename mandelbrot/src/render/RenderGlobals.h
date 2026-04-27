#pragma once

#include "ThreadPool.h"

namespace RenderGlobals {
    extern int width, height, outputWidth, outputHeight, aaPixels;
    extern bool useStreamIO, useThreads;

    bool setImageGlobals(int img_w, int img_h, int img_aaPixels = 1);
    void setUseThreads(bool enabled);

    [[nodiscard]] ThreadPool<> &getRenderPool(bool create = true);
    void forceKillRenderThreads();
}
