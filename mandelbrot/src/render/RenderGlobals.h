#pragma once

namespace RenderGlobals {
    extern int width, height;
    extern bool useStreamIO, useThreads;

    bool setImageGlobals(int img_w, int img_h);
}
