#include "RenderGlobals.h"

namespace RenderGlobals {
    int width = 0, height = 0;
    bool useStreamIo = false, useThreads = false;

    bool setImageGlobals(int img_w, int img_h) {
        if (img_w <= 0 || img_h <= 0) return false;

        width = img_w;
        height = img_h;

        return true;
    }
}