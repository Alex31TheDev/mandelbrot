#include "CallbackFormatter.h"

#include <cstdio>
#include <string>

#include "BackendAPI.h"
using namespace Backend;

#include "util/FormatUtil.h"

static void printAllocatedImageEvent(const ImageEvent &event) {
    if (event.downscaling) {
        printf("Supersampling: %.2fx (%dx%d -> %dx%d)\n",
            event.aaScale,
            event.width, event.height,
            event.outputWidth, event.outputHeight);

        printf("Memory required: %s (%s + %s)\n",
            FormatUtil::formatBufferSize(
                event.primaryBytes + event.secondaryBytes).c_str(),
            FormatUtil::formatBufferSize(event.primaryBytes).c_str(),
            FormatUtil::formatBufferSize(event.secondaryBytes).c_str());
        return;
    }

    printf("Memory required: %s\n",
        FormatUtil::formatBufferSize(event.primaryBytes).c_str());
}

void CallbackFormatter::bind(Session &session) const {
    Callbacks callbacks;

    callbacks.onProgress = [](const ProgressEvent &event) {
        printf("\r\x1b[KRendering: %3d%% | %.2f pixels/s",
            event.percentage, event.opsPerSecond);

        if (event.completed) {
            const std::string elapsed =
                FormatUtil::formatDuration(event.elapsedMs);
            printf(" (completed in: %s)\n", elapsed.c_str());
        }

        fflush(stdout);
        };

    callbacks.onImage = [](const ImageEvent &event) {
        switch (event.kind) {
            case ImageEventKind::allocated:
                printAllocatedImageEvent(event);
                return;

            case ImageEventKind::saved:
                if (event.path) {
                    printf("Successfully saved: %s (%dx%d)\n",
                        event.path, event.outputWidth, event.outputHeight);
                }
                return;
        }
        };

    callbacks.onInfo = [](const InfoEvent &event) {
        switch (event.kind) {
            case InfoEventKind::iterations:
                printf("Total iterations: %s | %.2f GI/s\n",
                    FormatUtil::formatNumber(event.totalIterations).c_str(),
                    event.opsPerSecond);
                return;
        }
        };

    callbacks.onDebug = [](const DebugEvent &event) {
        if (event.message) {
            fprintf(stderr, "%s\n", event.message);
        }
        };

    session.setCallbacks(callbacks);
}