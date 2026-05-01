#include "RenderGlobals.h"

#include <memory>
#include <stdexcept>
#include <tuple>

#include "image/Image.h"
#include "ThreadPool.h"

static std::unique_ptr<ThreadPool<>> _pool;

namespace RenderGlobals {
    int width = 0, height = 0, outputWidth = 0, outputHeight = 0, aaPixels = 1;
    bool useStreamIO = false, useThreads = false;

    [[nodiscard]] ThreadPool<> &getRenderPool(bool create) {
        if (create && !_pool) _pool = std::make_unique<ThreadPool<>>();

        if (!_pool) {
            throw std::runtime_error("Render thread pool is not initialized.");
        }

        return *_pool;
    }

    bool setImageGlobals(int img_w, int img_h, int img_aaPixels) {
        if (img_w <= 0 || img_h <= 0 || img_aaPixels <= 0) return false;

        outputWidth = img_w;
        outputHeight = img_h;
        aaPixels = img_aaPixels;

        std::tie(width, height) = Image::calcRenderDimensions(
            outputWidth, outputHeight, Image::calcAAScale(aaPixels));

        return true;
    }

    void setUseThreads(bool enabled) {
        if (useThreads == enabled) {
            if (useThreads) std::ignore = getRenderPool(true);
            return;
        } else {
            forceKillRenderThreads();

            useThreads = enabled;
            if (useThreads) std::ignore = getRenderPool(true);
        }
    }

    void forceKillRenderThreads() {
        if (!_pool) return;

        _pool->forceTerminate();
        _pool.reset(nullptr);
    }
}