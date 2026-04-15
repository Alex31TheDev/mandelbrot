#include "BackendApi.h"

#include <memory>
#include <string>
#include <string_view>

#include "util/ParserUtil.h"
#include "image/Image.h"
#include "render/RenderGlobals.h"
#include "render/RenderImage.h"
#include "scalar/ScalarGlobals.h"
#include "scalar/ScalarColors.h"
#include "vector/VectorGlobals.h"
#include "mpfr/MpfrGlobals.h"

using namespace RenderGlobals;
using namespace ScalarGlobals;
using namespace ParserUtil;

static Backend::Status makeFailure(std::string message) {
    return Backend::Status::failure(std::move(message));
}

static bool isValidFullInput(std::string_view value) {
    if (value.empty()) return false;

#if defined(USE_MPFR)
    try {
        static_cast<void>(mpfr::mpreal(std::string(value)));
        return true;
    } catch (...) {
        return false;
    }
#else
    bool ok = false;
    static_cast<void>(parseNumber<scalar_full_t>(std::string(value), std::ref(ok)));
    return ok;
#endif
}

static ScalarPaletteColor makePaletteColor(const Backend::PaletteEntry &entry) {
    ScalarPaletteColor color{};
    const ScalarColor parsed = ScalarColor::fromString(entry.color);
    color.R = parsed.R;
    color.G = parsed.G;
    color.B = parsed.B;
    color.length = entry.length;
    return color;
}

static bool isValidPaletteColor(const ScalarPaletteColor &entry) {
    return entry.R >= ZERO_H && entry.G >= ZERO_H &&
        entry.B >= ZERO_H && entry.length >= ZERO_H;
}

class MandelbrotBackendSession final : public Backend::Session {
public:
    MandelbrotBackendSession() {
        MPFRGlobals::initMPFR();
        colorMethod = static_cast<int>(Backend::ColorMethod::smooth_iterations);
        fractalType = static_cast<int>(Backend::FractalType::mandelbrot);
        setAllDefaults();
    }

    void setCallbacks(const Backend::Callbacks &callbacks) override {
        _callbacks = callbacks;
        if (_image) _image->setCallbacks(&_callbacks);
    }

    Backend::Status setImageSize(int imageWidth, int imageHeight, int aa) override {
        if (!setImageGlobals(imageWidth, imageHeight, aa)) {
            return makeFailure("Width, height, and aaPixels must be > 0.");
        }

        return Backend::Status::success();
    }

    void setUseThreads(bool enabled) override {
        useThreads = enabled;
    }

    Backend::Status setZoom(int iterCount, float zoomScale) override {
        if (!setZoomGlobals(iterCount, zoomScale)) {
            return makeFailure("Scale must be > -3.25.");
        }

        return Backend::Status::success();
    }

    Backend::Status setPoint(std::string_view real, std::string_view imag) override {
        if (!isValidFullInput(real) || !isValidFullInput(imag)) {
            return makeFailure("Point coordinates must be valid numbers.");
        }

        _pointReal = std::string(real);
        _pointImag = std::string(imag);

        setZoomPoints(
            PARSE_F(_pointReal.c_str()),
            PARSE_F(_pointImag.c_str()),
            seed_r,
            seed_i
        );

        return Backend::Status::success();
    }

    Backend::Status setSeed(std::string_view real, std::string_view imag) override {
        if (!isValidFullInput(real) || !isValidFullInput(imag)) {
            return makeFailure("Seed coordinates must be valid numbers.");
        }

        _seedReal = std::string(real);
        _seedImag = std::string(imag);

        setZoomPoints(
            point_r,
            point_i,
            PARSE_F(_seedReal.c_str()),
            PARSE_F(_seedImag.c_str())
        );

        return Backend::Status::success();
    }

    Backend::Status setFractalType(Backend::FractalType type) override {
        fractalType = static_cast<int>(type);
        return Backend::Status::success();
    }

    void setFractalMode(bool juliaSet, bool inverse) override {
        ScalarGlobals::setFractalType(juliaSet, inverse);
    }

    Backend::Status setFractalExponent(std::string_view exponent) override {
        if (!isValidFullInput(exponent)) {
            return makeFailure("Fractal exponent must be a valid number.");
        }

        _exponent = std::string(exponent);
        if (!ScalarGlobals::setFractalExponent(PARSE_F(_exponent.c_str()))) {
            return makeFailure("Fractal exponent must be > 1.");
        }

        return Backend::Status::success();
    }

    Backend::Status setColorMethod(Backend::ColorMethod method) override {
        colorMethod = static_cast<int>(method);
        return Backend::Status::success();
    }

    Backend::Status setColorFormula(float freqR, float freqG, float freqB,
        float freqMult) override {
        if (!setColorGlobals(freqR, freqG, freqB, freqMult)) {
            return makeFailure("Frequency multiplier must be non-zero.");
        }

        return Backend::Status::success();
    }

    Backend::Status setPalette(const Backend::PaletteConfig &paletteConfig) override {
        if (paletteConfig.totalLength <= ZERO_H || paletteConfig.offset < ZERO_H) {
            return makeFailure("Palette length must be > 0 and offset must be >= 0.");
        }

        std::vector<ScalarPaletteColor> entries;
        entries.reserve(paletteConfig.entries.size());

        for (const auto &entry : paletteConfig.entries) {
            ScalarPaletteColor parsed = makePaletteColor(entry);
            if (!isValidPaletteColor(parsed)) {
                return makeFailure(
                    "Palette entries must use valid #RRGGBB colors and lengths >= 0.");
            }

            entries.push_back(parsed);
        }

        if (!setPaletteGlobals(entries, paletteConfig.totalLength, paletteConfig.offset)) {
            return makeFailure("Palette must contain at least 2 valid entries.");
        }

        return Backend::Status::success();
    }

    Backend::Status setLight(float real, float imag) override {
        if (!setLightGlobals(real, imag)) {
            return makeFailure("Light vector must be non-zero.");
        }

        return Backend::Status::success();
    }

    Backend::Status render() override {
        auto status = ensureImage();
        if (!status) return status;

        VectorGlobals::initVectors();
        MPFRGlobals::initMPFRValues(_pointReal.c_str(), _pointImag.c_str());

        renderImage(_image.get(), &_callbacks,
            static_cast<bool>(_callbacks.onProgress),
            static_cast<bool>(_callbacks.onInfo));

        return Backend::Status::success();
    }

    void clearImage() override {
        if (_image) _image->clear();
    }

    Backend::ImageView imageView() const override {
        if (!_image) return {};

        return {
            .pixels = _image->pixels(),
            .outputPixels = _image->outputPixels(),
            .aspect = _image->aspect(),
            .width = _image->width(),
            .height = _image->height(),
            .outputWidth = _image->outputWidth(),
            .outputHeight = _image->outputHeight(),
            .strideWidth = _image->strideWidth(),
            .tailBytes = _image->tailBytes(),
            .size = _image->size(),
            .downscaling = _image->downscaling(),
            .aaScale = _image->aaScale()
        };
    }

    Backend::Status saveImage(std::string_view path, bool appendDate,
        std::string_view type) override {
        if (!_image) {
            return makeFailure("No rendered image is available.");
        }

        if (!_image->saveToFile(std::string(path), appendDate, std::string(type))) {
            return makeFailure("Failed to save image.");
        }

        return Backend::Status::success();
    }

private:
    Backend::Callbacks _callbacks;
    std::unique_ptr<Image> _image;

    std::string _pointReal = "0";
    std::string _pointImag = "0";
    std::string _seedReal = "0";
    std::string _seedImag = "0";
    std::string _exponent = "2";
    int _lastAAPixels = 1;

    Backend::Status ensureImage() {
        const bool needsNewImage = !_image ||
            _image->outputWidth() != outputWidth ||
            _image->outputHeight() != outputHeight ||
            _lastAAPixels != aaPixels;

        if (needsNewImage) {
            _image = Image::create(outputWidth, outputHeight, useThreads, aaPixels);
            if (!_image) return makeFailure("Failed to allocate image.");
            _image->setCallbacks(&_callbacks);
            _lastAAPixels = aaPixels;
        } else {
            _image->clear();
        }

        return Backend::Status::success();
    }
};

extern "C" __declspec(dllexport) Backend::Session *mandelbrot_backend_create() {
    return new MandelbrotBackendSession();
}

extern "C" __declspec(dllexport) void mandelbrot_backend_destroy(
    Backend::Session *session) {
    delete session;
}