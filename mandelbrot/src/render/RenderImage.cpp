#include "RenderImage.h"

#include <algorithm>
#include <thread>
#include <memory>
#include <mutex>

#include "../image/Image.h"
#include "RenderProgress.h"

#include "RenderGlobals.h"
using namespace RenderGlobals;

#if defined(USE_SCALAR)
#include "../scalar/ScalarRenderer.h"
constexpr auto renderPixel = &ScalarRenderer::renderPixelScalar;
#elif defined(USE_VECTORS)
#include "../vector/VectorTypes.h"
#include "../vector/VectorRenderer.h"
constexpr auto renderPixel = &VectorRenderer::renderPixelSimd;
#elif defined(USE_MPFR)
#include "../mpfr/MpfrRenderer.h"
constexpr auto renderPixel = &MpfrRenderer::renderPixelMpfr;
#else
#error "No renderer implementation selected. (define USE_SCALAR, USE_VECTORS, or USE_MPFR)"
#endif

#if defined(USE_SCALAR) || defined(USE_VECTORS)
#include "../scalar/ScalarCoords.h"
constexpr auto imagCenterCoord = &getCenterImag;
#elif defined(USE_MPFR)
#include "../mpfr/MpfrCoords.h"
constexpr auto imagCenterCoord = &getCenterImag_mp;
#endif

#include "ThreadPool.h"

constexpr int MAX_TASK_COUNT = 4096;

static ThreadPool<> &getThreadPool() {
    static std::unique_ptr<ThreadPool<>> pool;
    static std::once_flag initFlag;

    std::call_once(initFlag, [&]() {
        pool = std::make_unique<ThreadPool<>>(std::thread::hardware_concurrency());
        });

    return *pool;
}

static void renderStrip(Image *image,
    int start_y, int end_y,
    RenderProgress *progress = nullptr) {
    if (image == nullptr) return;
    int pos = start_y * width * Image::STRIDE;

    for (int y = start_y; y < end_y; y++) {
        const auto ci = imagCenterCoord(y);

#if defined(USE_SCALAR) || defined(USE_MPFR)
        for (int x = 0; x < width; x++) {
            renderPixel(image->pixels(), pos, x, ci);
        }
#elif defined(USE_VECTORS)
        for (int x = 0; x < width; x += SIMD_FULL_WIDTH) {
            const int pixels_left = width - x;
            const int simd_width = pixels_left < SIMD_FULL_WIDTH ?
                pixels_left : SIMD_FULL_WIDTH;

            renderPixel(image->pixels(), pos, simd_width, x, ci);
        }
#endif

        if (progress) progress->update();
    }
}

static void renderImageSequential(Image *image) {
    if (image == nullptr) return;

    RenderProgress progress(height);
    renderStrip(image, 0, height, &progress);
    progress.complete(true);
}

static void renderImageParallel(Image *image) {
    if (image == nullptr) return;
    ThreadPool<> &pool = getThreadPool();

    if (pool.size() == 0 || pool.size() == 1) {
        renderImageSequential(image);
        return;
    }

    RenderProgress progress(height);

    const int taskCount = std::min(height, MAX_TASK_COUNT);

    const int rowsPerTask = height / taskCount;
    const int extraRows = height % taskCount;

    int start_y = 0;

    for (int i = 0; i < taskCount; i++) {
        const int chunkRows = rowsPerTask + (i < extraRows ? 1 : 0);
        const int end_y = start_y + chunkRows;

        pool.enqueueDetach([=, &progress]() {
            renderStrip(image, start_y, end_y, &progress);
            });

        start_y = end_y;
    }

    pool.waitForTasks();
    progress.complete(true);
}

void renderImage(Image *image) {
    if (useThreads) renderImageParallel(image);
    else renderImageSequential(image);
}