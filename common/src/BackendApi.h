#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace Backend {
    enum class ColorMethod {
        iterations = 0,
        smooth_iterations = 1,
        palette = 2,
        light = 3
    };

    enum class FractalType {
        mandelbrot = 0,
        perpendicular = 1,
        burningship = 2
    };

    struct Status {
        bool ok = true;
        std::string message;

        explicit operator bool() const { return ok; }

        static Status success() { return {}; }
        static Status failure(std::string message) {
            Status status;
            status.ok = false;
            status.message = std::move(message);
            return status;
        }
    };

    struct PaletteEntry {
        std::string color;
        float length = 1.0f;
    };

    struct PaletteConfig {
        float totalLength = 10.0f;
        float offset = 0.0f;
        std::vector<PaletteEntry> entries;
    };

    struct ProgressEvent {
        int percentage = 0;
        double opsPerSecond = 0.0;
        int64_t elapsedMs = 0;
        uint64_t completedWork = 0;
        uint64_t totalWork = 0;
        bool completed = false;
    };

    enum class ImageEventKind {
        allocated,
        saved
    };

    struct ImageEvent {
        ImageEventKind kind = ImageEventKind::allocated;
        float aaScale = 1.0f;
        float aspect = 0.0f;
        bool downscaling = false;
        int32_t width = 0;
        int32_t height = 0;
        int32_t outputWidth = 0;
        int32_t outputHeight = 0;
        size_t primaryBytes = 0;
        size_t secondaryBytes = 0;
        const char *path = nullptr;
    };

    enum class InfoEventKind {
        iterations
    };

    struct InfoEvent {
        InfoEventKind kind = InfoEventKind::iterations;
        uint64_t totalIterations = 0;
        double opsPerSecond = 0.0;
        int64_t elapsedMs = 0;
    };

    struct DebugEvent {
        const char *message = nullptr;
    };

    struct Callbacks {
        std::function<void(const ProgressEvent &)> onProgress;
        std::function<void(const ImageEvent &)> onImage;
        std::function<void(const InfoEvent &)> onInfo;
        std::function<void(const DebugEvent &)> onDebug;
    };

    struct ImageView {
        const uint8_t *pixels = nullptr;
        const uint8_t *outputPixels = nullptr;
        float aspect = 0.0f;
        int32_t width = 0;
        int32_t height = 0;
        int32_t outputWidth = 0;
        int32_t outputHeight = 0;
        int32_t strideWidth = 0;
        int32_t tailBytes = 0;
        size_t size = 0;
        bool downscaling = false;
        float aaScale = 1.0f;
    };

    class Session {
    public:
        virtual ~Session() = default;

        virtual void setCallbacks(const Callbacks &callbacks) = 0;

        virtual Status setImageSize(int width, int height, int aaPixels) = 0;
        virtual void setUseThreads(bool useThreads) = 0;

        virtual Status setZoom(int iterCount, float zoom) = 0;
        virtual Status setPoint(std::string_view real, std::string_view imag) = 0;
        virtual Status setSeed(std::string_view real, std::string_view imag) = 0;
        virtual Status setFractalType(FractalType fractalType) = 0;
        virtual void setFractalMode(bool isJuliaSet, bool isInverse) = 0;
        virtual Status setFractalExponent(std::string_view exponent) = 0;

        virtual Status setColorMethod(ColorMethod colorMethod) = 0;
        virtual Status setColorFormula(float freqR, float freqG, float freqB,
            float freqMult) = 0;
        virtual Status setPalette(const PaletteConfig &palette) = 0;
        virtual Status setLight(float real, float imag) = 0;

        virtual Status render() = 0;
        virtual void clearImage() = 0;

        virtual ImageView imageView() const = 0;
        virtual Status saveImage(std::string_view path, bool appendDate,
            std::string_view type) = 0;
    };
}
