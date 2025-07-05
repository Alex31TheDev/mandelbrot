#include "RenderImage.h"

#include <cstdio>
#include <cstdint>
#include <cinttypes>

#include <memory>
#include <mutex>
#include <numeric>

#include "../image/Image.h"
#include "ProgressTracker.h"
#include "ThreadPool.h"

#include "RenderGlobals.h"
using namespace RenderGlobals;

#if defined(USE_SCALAR)

#include "../scalar/ScalarCoords.h"
#include "../scalar/ScalarRenderer.h"

constexpr auto imagCenterCoord = &getCenterImag;
constexpr auto renderPixel = &ScalarRenderer::renderPixelScalar;

#elif defined(USE_VECTORS)

#include "../vector/VectorTypes.h"

#include "../vector/VectorCoords.h"
#include "../vector/VectorRenderer.h"

constexpr auto imagCenterCoord = &getCenterImag_vec;
constexpr auto renderPixel = &VectorRenderer::renderPixelSIMD;

#elif defined(USE_MPFR)

#include "../mpfr/MPFRCoords.h"
#include "../mpfr/MPFRRenderer.h"

constexpr auto imagCenterCoord = &getCenterImag_mp;
constexpr auto renderPixel = &MPFRRenderer::renderPixelMPFR;

#else
#error "No renderer implementation selected. (define USE_SCALAR, USE_VECTORS, or USE_MPFR)"
#endif

#include "../util/FormatUtil.h"

const ProgressConfig progressConfig = {
    .progressName = "Rendering",
    .opsName = "rows"
};

constexpr int MIN_THREADING_HEIGHT = 50;
constexpr int MAX_TASK_COUNT = 4096;

inline std::unique_ptr<ProgressTracker>
createProgressTracker(bool trackProgress) {
    if (trackProgress) {
        return std::make_unique<ProgressTracker>(height, useThreads,
            progressConfig);
    } else return nullptr;
}

inline ThreadPool<> &getThreadPool() {
    static ThreadPool<> pool;
    return pool;
}

static void renderStrip(
    Image *image, int start_y, int end_y,
    uint64_t *totalIterCount, ProgressTracker *progress
) {
    uint64_t iterCount = 0, totalCount = 0;
    uint64_t *iterPtr = totalIterCount ? &iterCount : nullptr;

    size_t pos = static_cast<size_t>(start_y) * image->strideWidth();

    for (int y = start_y; y < end_y; y++) {
#if defined(USE_SCALAR) || defined(USE_MPFR)
        const auto ci = imagCenterCoord(y);

        for (int x = 0; x < width; x++) {
            renderPixel(image->pixels(), pos, x, ci, iterPtr);
            totalCount += iterCount;
        }
#elif defined(USE_VECTORS)
        const simd_full_t ci = imagCenterCoord(SIMD_SET1_F(y));

        for (int x = 0; x < width; x += SIMD_FULL_WIDTH) {
            const int pixelsLeft = width - x;
            const int simdWidth =
                pixelsLeft < SIMD_FULL_WIDTH ?
                pixelsLeft : SIMD_FULL_WIDTH;

            renderPixel(
                image->pixels(), pos,
                simdWidth, x, ci, iterPtr
            );

            totalCount += iterCount;
        }
#endif

        pos += image->tailBytes();
        if (progress) progress->update();
    }

    if (totalIterCount) *totalIterCount = totalCount;
}

inline void printIterations(uint64_t iter, ProgressTracker::SU time) {
    const double gigaIter = static_cast<double>(iter) / 1.0e9;
    const double timeSec = static_cast<double>(time.count()) /
        ProgressTracker::SHORT_UNIT_SCALE;

    const double iterSec = gigaIter / timeSec;
    printf("Total iterations: %s | %.2f GI/s\n",
        FormatUtil::formatNumber(iter).c_str(), iterSec);
}

static void renderImageSequential(
    Image *image,
    bool trackProgress, bool trackIterations
) {
    auto progress = createProgressTracker(trackProgress);

    uint64_t iterCount = 0;
    uint64_t *iterPtr = trackIterations ? &iterCount : nullptr;

    renderStrip(image, 0, height, iterPtr, progress.get());

    if (trackProgress) progress->complete();
    if (trackIterations) printIterations(iterCount, progress->elapsed());
}

static void renderImageParallel(
    Image *image,
    bool trackProgress, bool trackIterations
) {
    auto &pool = getThreadPool();

    if (height < MIN_THREADING_HEIGHT ||
        pool.size() <= 1) {
        renderImageSequential(image, trackProgress, trackIterations);
        return;
    }

    auto progress = createProgressTracker(trackProgress);

    const int taskCount = (height < MAX_TASK_COUNT)
        ? height : MAX_TASK_COUNT;

    const int rowsPerTask = height / taskCount;
    const int extraRows = height % taskCount;

    auto iterCounts = trackIterations
        ? std::make_unique<uint64_t[]>(taskCount)
        : nullptr;

    int start_y = 0;

    for (int i = 0; i < taskCount; i++) {
        const int chunkRows = rowsPerTask + (i < extraRows ? 1 : 0);
        const int end_y = start_y + chunkRows;

        uint64_t *iterPtr = trackIterations ? &iterCounts[i] : nullptr;

        pool.enqueueDetach(
            [=, &progress]() { renderStrip(image, start_y, end_y,
                iterPtr, progress.get()); }
        );

        start_y = end_y;
    }

    pool.waitForTasks();

    const uint64_t totalCount = trackIterations
        ? std::accumulate(iterCounts.get(),
            iterCounts.get() + taskCount, 0Ui64) : 0;

    if (trackProgress) progress->complete();
    if (trackIterations) printIterations(totalCount, progress->elapsed());
}

void renderImage(
    Image *image,
    bool trackProgress, bool trackIterations
) {
    auto render = useThreads
        ? renderImageParallel
        : renderImageSequential;

    render(image, trackProgress, trackIterations);
}