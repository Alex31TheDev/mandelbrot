#include "RenderImage.h"

#include <cstdint>

#include <memory>
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
        unsigned y, OptionalIterationStats iterStats
        ) const {
        const auto ci = getCenterImag(y);

        for (unsigned x = xstart; x <= xend; ++x) {
            ScalarRenderer::renderPixelScalar(
                image->pixels(), pos,
                static_cast<int>(x), ci,
                iterStats, static_cast<int>(y)
            );
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
        unsigned y, OptionalIterationStats iterStats
        ) const {
        const simd_full_t ci = getCenterImag_vec(SIMD_SET1_F(y));

        for (unsigned x = xstart; x <= xend; x += chunkWidth) {
            const int pixelsLeft = static_cast<int>(xend - x + 1);
            const int simdWidth = pixelsLeft < chunkWidth
                ? pixelsLeft
                : chunkWidth;

            VectorRenderer::renderPixelSIMD(
                image->pixels(), pos,
                simdWidth, x, ci,
                iterStats, static_cast<int>(y)
            );
        }
    }
};

#elif defined(USE_MPFR)

#include "../mpfr/MPFRRenderer.h"

struct RowRenderer {
    std::shared_ptr<const MPFRRenderer::PerturbationReference> reference;

    RowRenderer() {
        reference = MPFRRenderer::preparePerturbationReference();
    }

    void operator()(
        Image *image, size_t &pos, unsigned xstart, unsigned xend,
        unsigned y, OptionalIterationStats iterStats
        ) {
        for (unsigned x = xstart; x <= xend; ++x) {
            MPFRRenderer::renderPixelMPFR(
                image->pixels(), pos,
                static_cast<int>(x),
                static_cast<int>(y), iterStats,
                reference.get()
            );
        }
    }
};

#elif defined(USE_QD)

#include "../qd/QDCoords.h"
#include "../qd/QDRenderer.h"

struct RowRenderer {
    qd_real ci;

    void operator()(
        Image *image, size_t &pos, unsigned xstart, unsigned xend,
        unsigned y, OptionalIterationStats iterStats
        ) {
        getCenterImag_qd(ci, static_cast<int>(y));

        for (unsigned x = xstart; x <= xend; ++x) {
            QDRenderer::renderPixelQD(
                image->pixels(), pos,
                static_cast<int>(x), ci,
                iterStats, static_cast<int>(y)
            );
        }
    }
};

#else
#error "No renderer implementation selected. (define USE_SCALAR, USE_VECTORS, USE_MPFR, or USE_QD)"
#endif

constexpr int MIN_THREADING_HEIGHT = 50;
constexpr int MAX_TASK_COUNT = 4096;

static std::unique_ptr<ProgressTracker>
createProgressTracker(bool trackProgress,
    const Backend::Callbacks *callbacks = nullptr) {
    if (trackProgress) {
        const ProgressOptions options{
            .callbacks = callbacks
        };
        return std::make_unique<ProgressTracker>(width * height,
            useThreads, options);
    } else return nullptr;
}

static std::unique_ptr<ThreadPool<>> &getThreadPool(bool create = true) {
    static std::unique_ptr<ThreadPool<>> pool = std::make_unique<ThreadPool<>>();
    if (create && !pool) {
        pool = std::make_unique<ThreadPool<>>();
    }
    return pool;
}

void forceKillRenderThreads() {
    auto &pool = getThreadPool(false);
    if (!pool) return;
    pool->forceTerminate();
    pool.reset();
}

static void renderStrip(
    Image *image,
    unsigned xstart, unsigned xend,
    unsigned ystart, unsigned yend,
    ProgressTracker *progress,
    OptionalIterationStats iterStats
) {
    RowRenderer renderer;

    const unsigned rowWidth = xend - xstart + 1;

    for (unsigned y = ystart; y <= yend; ++y) {
        size_t pos = static_cast<size_t>(y) * image->strideWidth() +
            static_cast<size_t>(xstart) * Image::STRIDE;

        renderer(
            image, pos, xstart, xend,
            y, iterStats
        );

        pos += image->tailBytes();
        if (progress) progress->update(rowWidth);
    }
}

static void emitIterations(const Backend::Callbacks *callbacks,
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
    bool trackProgress, bool trackIterations,
    OptionalIterationStats iterStats
) {
    auto progress = createProgressTracker(trackProgress, callbacks);

    RenderIterationStats localStats;
    if (trackIterations && !iterStats) iterStats = std::ref(localStats);

    renderStrip(image, 0, width - 1, 0, height - 1, progress.get(),
        trackIterations ? iterStats : std::nullopt);

    if (trackProgress) progress->complete();
    if (trackIterations) {
        emitIterations(callbacks, iterStats->get().totalIterations,
            progress->elapsed());
    }
}

static void renderImageParallel(
    Image *image,
    const Backend::Callbacks *callbacks,
    bool trackProgress, bool trackIterations,
    OptionalIterationStats iterStats
) {
    auto &pool = *getThreadPool();

    if (height < MIN_THREADING_HEIGHT ||
        pool.size() <= 1) {
        renderImageSequential(image, callbacks, trackProgress,
            trackIterations, iterStats);
        return;
    }

    auto progress = createProgressTracker(trackProgress, callbacks);
    const int taskCount = (height < MAX_TASK_COUNT)
        ? height : MAX_TASK_COUNT;

    const int rowsPerTask = height / taskCount;
    const int extraRows = height % taskCount;

    auto stats = trackIterations
        ? std::make_unique<RenderIterationStats[]>(taskCount)
        : nullptr;

    int startY = 0;

    for (int i = 0; i < taskCount; ++i) {
        const int chunkRows = rowsPerTask + (i < extraRows ? 1 : 0);
        const int endY = startY + chunkRows;

        OptionalIterationStats taskStats =
            stats ? OptionalIterationStats(std::ref(stats[i])) :
            std::nullopt;

        pool.enqueueDetach([=, &progress]() {
            renderStrip(image,
                0, width - 1,
                startY, endY - 1,
                progress.get(), taskStats);
            });

        startY = endY;
    }

    pool.waitForTasks();

    if (trackProgress) progress->complete();
    if (trackIterations) {
        RenderIterationStats mergedStats;
        for (int i = 0; i < taskCount; ++i) mergedStats.merge(stats[i]);
        if (iterStats) iterStats->get().merge(mergedStats);

        emitIterations(callbacks, mergedStats.totalIterations,
            progress->elapsed());
    }
}

void renderImage(
    Image *image,
    const Backend::Callbacks *callbacks,
    bool trackProgress, bool trackIterations,
    OptionalIterationStats iterStats
) {
    auto render = useThreads
        ? renderImageParallel
        : renderImageSequential;

    render(image, callbacks, trackProgress, trackIterations, iterStats);
}
