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

struct RowRenderer {
    void operator()(
        Image *image, size_t &pos, unsigned xstart, unsigned xend,
        unsigned y, uint64_t *iterPtr, uint64_t &totalCount
    ) const {
        const auto ci = getCenterImag(y);

        for (unsigned x = xstart; x <= xend; ++x) {
            ScalarRenderer::renderPixelScalar(
                image->pixels(), pos,
                static_cast<int>(x), ci, iterPtr
            );
            if (iterPtr) totalCount += *iterPtr;
        }
    }
};

#elif defined(USE_VECTORS)

#include "../scalar/ScalarGlobals.h"
#include "../vector/VectorTypes.h"

#include "../vector/VectorCoords.h"
#include "../vector/VectorRenderer.h"

struct RowRenderer {
    int chunkWidth;

    RowRenderer() : chunkWidth(
        ScalarGlobals::useQuadPath() ? 4 * SIMD_FULL_WIDTH : SIMD_FULL_WIDTH
    ) {}

    void operator()(
        Image *image, size_t &pos, unsigned xstart, unsigned xend,
        unsigned y, uint64_t *iterPtr, uint64_t &totalCount
    ) const {
        const simd_full_t ci = getCenterImag_vec(SIMD_SET1_F(y));

        for (unsigned x = xstart; x <= xend; x += chunkWidth) {
            const int pixelsLeft = static_cast<int>(xend - x + 1);
            const int simdWidth = pixelsLeft < chunkWidth
                ? pixelsLeft
                : chunkWidth;

            VectorRenderer::renderPixelSIMD(
                image->pixels(), pos,
                simdWidth, x, ci, iterPtr
            );
            if (iterPtr) totalCount += *iterPtr;
        }
    }
};

#elif defined(USE_MPFR)

#include "../mpfr/MPFRCoords.h"
#include "../mpfr/MPFRRenderer.h"
#include "../mpfr/MPFRTypes.h"

struct RowRenderer {
    mpfr_t ci;

    RowRenderer() {
        MPFRTypes::initRaw(ci);
    }

    ~RowRenderer() {
        mpfr_clear(ci);
    }

    void operator()(
        Image *image, size_t &pos, unsigned xstart, unsigned xend,
        unsigned y, uint64_t *iterPtr,  uint64_t &totalCount
    ) {
        getCenterImag_mp(ci, static_cast<int>(y));

        for (unsigned x = xstart; x <= xend; ++x) {
            MPFRRenderer::renderPixelMPFR(
                image->pixels(), pos,
                static_cast<int>(x), ci, iterPtr
            );
            if (iterPtr) totalCount += *iterPtr;
        }
    }
};

#else
#error "No renderer implementation selected. (define USE_SCALAR, USE_VECTORS, or USE_MPFR)"
#endif

#include "util/FormatUtil.h"

const ProgressConfig progressConfig = {
    .progressName = "Rendering",
    .opsName = "pixels"
};

constexpr int MIN_THREADING_HEIGHT = 50;
constexpr int MAX_TASK_COUNT = 4096;

inline std::unique_ptr<ProgressTracker>
createProgressTracker(bool trackProgress,
    const Backend::Callbacks *callbacks = nullptr) {
    if (trackProgress) {
        auto config = progressConfig;
        config.callbacks = callbacks;

        return std::make_unique<ProgressTracker>(width * height,
            useThreads, config);
    } else return nullptr;
}

inline ThreadPool<> &getThreadPool() {
    static ThreadPool<> pool;
    return pool;
}

static void renderStrip(
    Image *image,
    unsigned xstart, unsigned xend,
    unsigned ystart, unsigned yend,
    uint64_t *totalIterCount, ProgressTracker *progress
) {
    RowRenderer renderer;

    uint64_t iterCount = 0, totalCount = 0;
    uint64_t *iterPtr = totalIterCount ? &iterCount : nullptr;

    const unsigned rowWidth = xend - xstart + 1;

    for (unsigned y = ystart; y <= yend; ++y) {
        size_t pos = static_cast<size_t>(y) * image->strideWidth() +
            static_cast<size_t>(xstart) * Image::STRIDE;

        renderer(
            image, pos, xstart, xend,
            y, iterPtr, totalCount
        );

        pos += image->tailBytes();
        if (progress) progress->update(rowWidth);
    }

    if (totalIterCount) *totalIterCount = totalCount;
}

inline void emitIterations(const Backend::Callbacks *callbacks,
    uint64_t iter, ProgressTracker::SU time) {
    if (!callbacks || !callbacks->onInfo) return;

    const double gigaIter = static_cast<double>(iter) / 1.0e9;
    const double timeSec = static_cast<double>(time.count()) /
        ProgressTracker::SHORT_UNIT_SCALE;
    const double iterSec = timeSec > 0.0 ? gigaIter / timeSec : 0.0;

    const Backend::InfoEvent event = {
        .kind = Backend::InfoEventKind::iterations,
        .totalIterations = iter,
        .opsPerSecond = iterSec,
        .elapsedMs = static_cast<int64_t>(time.count())
    };

    callbacks->onInfo(event);
}

static void renderImageSequential(
    Image *image,
    const Backend::Callbacks *callbacks,
    bool trackProgress, bool trackIterations
) {
    auto progress = createProgressTracker(trackProgress, callbacks);

    uint64_t iterCount = 0;
    uint64_t *iterPtr = trackIterations ? &iterCount : nullptr;

    renderStrip(image, 0, width - 1, 0, height - 1, iterPtr, progress.get());

    if (trackProgress) progress->complete();
    if (trackIterations) emitIterations(callbacks, iterCount, progress->elapsed());
}

static void renderImageParallel(
    Image *image,
    const Backend::Callbacks *callbacks,
    bool trackProgress, bool trackIterations
) {
    auto &pool = getThreadPool();

    if (height < MIN_THREADING_HEIGHT ||
        pool.size() <= 1) {
        renderImageSequential(image, callbacks, trackProgress, trackIterations);
        return;
    }

    auto progress = createProgressTracker(trackProgress, callbacks);
    const int taskCount = (height < MAX_TASK_COUNT)
        ? height : MAX_TASK_COUNT;

    const int rowsPerTask = height / taskCount;
    const int extraRows = height % taskCount;

    auto iterCounts = trackIterations
        ? std::make_unique<uint64_t[]>(taskCount)
        : nullptr;

    int startY = 0;

    for (int i = 0; i < taskCount; ++i) {
        const int chunkRows = rowsPerTask + (i < extraRows ? 1 : 0);
        const int endY = startY + chunkRows;
        uint64_t *iterPtr = trackIterations ? &iterCounts[i] : nullptr;

        pool.enqueueDetach([=, &progress]() {
            renderStrip(image,
                0, width - 1,
                startY, endY - 1,
                iterPtr, progress.get());
            });

        startY = endY;
    }

    pool.waitForTasks();

    const uint64_t totalCount = trackIterations
        ? std::accumulate(iterCounts.get(),
            iterCounts.get() + taskCount, 0Ui64)
        : 0;

    if (trackProgress) progress->complete();
    if (trackIterations)
        emitIterations(callbacks, totalCount, progress->elapsed());
}

void renderImage(
    Image *image,
    const Backend::Callbacks *callbacks,
    bool trackProgress, bool trackIterations
) {
    auto render = useThreads
        ? renderImageParallel
        : renderImageSequential;

    render(image, callbacks, trackProgress, trackIterations);
}
