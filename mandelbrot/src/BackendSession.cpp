#include "BackendAPI.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

#include "util/ParserUtil.h"

#include "image/Image.h"

#include "render/RenderGlobals.h"
#include "render/RenderImage.h"

#include "scalar/ScalarGlobals.h"
#include "scalar/ScalarPrecision.h"
#include "scalar/ScalarColors.h"
#include "scalar/ScalarCoords.h"
#include "scalar/ScalarRenderer.h"

#include "vector/VectorGlobals.h"

#include "mpfr/MPFRGlobals.h"
#include "mpfr/MPFRCoords.h"

#include "qd/QDGlobals.h"
#include "qd/QDCoords.h"
#include "qd/QDTypes.h"

#if defined(USE_KF2)
#include "kf2/KF2Globals.h"
#include "kf2/KF2Renderer.h"
#endif

using namespace RenderGlobals;
using namespace ScalarGlobals;
using namespace ParserUtil;

static Backend::Status makeFailure(std::string message) {
    return Backend::Status::failure(std::move(message));
}

static bool isValidFullInput(const std::string &value) {
    if (value.empty()) return false;

#if defined(USE_MPFR)
    return MPFRTypes::isValidNumber(value.c_str());
#elif defined(USE_QD)
    return QDTypes::isValidNumber(value.c_str());
#else
    bool ok = false;
    parseNumber<scalar_full_t>(value, std::ref(ok));
    return ok;
#endif
}

static bool parseZoomInput(const std::string &value, scalar_half_t &out) {
    if (value.empty()) return false;

#if defined(USE_MPFR)
    mpfr_t zoomMp;
    mpfr_init2(zoomMp, MPFRTypes::precisionBits);
    const bool ok = MPFRTypes::parseNumber(zoomMp, value.c_str());
    if (ok) {
        out = static_cast<scalar_half_t>(mpfr_get_d(zoomMp, MPFRTypes::ROUNDING));
    }
    mpfr_clear(zoomMp);
    return ok;
#elif defined(USE_QD)
    qd_real zoomQD;
    if (!QDTypes::parseNumber(zoomQD, value.c_str())) return false;
    out = static_cast<scalar_half_t>(to_double(zoomQD));
    return true;
#else
    bool ok = false;
    out = parseNumber<scalar_half_t>(value, std::ref(ok), out);
    return ok;
#endif
}

static bool isZoomAboveMinimum(const std::string &value) {
    if (value.empty()) return false;

#if defined(USE_MPFR)
    mpfr_t zoomMp;
    mpfr_t minMp;
    mpfr_inits2(MPFRTypes::precisionBits, zoomMp, minMp, static_cast<mpfr_ptr>(nullptr));
    const bool ok = MPFRTypes::parseNumber(zoomMp, value.c_str());
    mpfr_set_d(minMp, -3.25, MPFRTypes::ROUNDING);
    const bool valid = ok && mpfr_greater_p(zoomMp, minMp);
    mpfr_clears(zoomMp, minMp, static_cast<mpfr_ptr>(nullptr));
    return valid;
#elif defined(USE_QD)
    qd_real zoomQD;
    if (!QDTypes::parseNumber(zoomQD, value.c_str())) return false;
    return zoomQD > qd_real(-3.25);
#else
    bool ok = false;
    const scalar_half_t parsed = parseNumber<scalar_half_t>(value, std::ref(ok), 0);
    return ok && std::isfinite(static_cast<double>(parsed)) &&
        parsed > static_cast<scalar_half_t>(-3.25);
#endif
}

static constexpr double minSessionZoom = -3.2499;

static ScalarPaletteColor makePaletteColor(const Backend::PaletteRGBEntry &entry) {
    ScalarPaletteColor color{};
    static_cast<ScalarColor &>(color) = { entry.R, entry.G, entry.B };

    color.length = entry.length;
    return color;
}

static ScalarPaletteColor makePaletteColor(const Backend::PaletteHexEntry &entry) {
    ScalarPaletteColor color{};
    static_cast<ScalarColor &>(color) = ScalarColor::fromString(entry.color);

    color.length = entry.length;
    return color;
}

static Backend::ImageView makeImageView(const Image *image) {
    if (!image) return {};

    return {
        .pixels = image->pixels(),
        .outputPixels = image->outputPixels(),
        .aspect = image->aspect(),
        .width = image->width(),
        .height = image->height(),
        .outputWidth = image->outputWidth(),
        .outputHeight = image->outputHeight(),
        .strideWidth = image->strideWidth(),
        .tailBytes = image->tailBytes(),
        .size = image->size(),
        .downscaling = image->downscaling(),
        .aaScale = image->aaScale()
    };
}

static bool scaleZoomText(const std::string &sourceZoom,
    double scaleMultiplier, std::string &zoomText) {
    if (!(scaleMultiplier > 0.0) || !std::isfinite(scaleMultiplier)) {
        return false;
    }

#if defined(USE_MPFR)
    mpfr_t zoomMp, scaleMp, deltaMp;
    mpfr_inits2(MPFRTypes::precisionBits,
        zoomMp, scaleMp, deltaMp, static_cast<mpfr_ptr>(nullptr));

    const bool ok = MPFRTypes::parseNumber(zoomMp, sourceZoom.c_str());
    if (ok) {
        mpfr_set_d(scaleMp, scaleMultiplier, MPFRTypes::ROUNDING);
        mpfr_log10(deltaMp, scaleMp, MPFRTypes::ROUNDING);
        mpfr_sub(zoomMp, zoomMp, deltaMp, MPFRTypes::ROUNDING);
        if (mpfr_less_p(zoomMp, MPFRTypes::toNumber(minSessionZoom))) {
            mpfr_set_d(zoomMp, minSessionZoom, MPFRTypes::ROUNDING);
        }
        zoomText = MPFRTypes::toString(zoomMp);
    }

    mpfr_clears(zoomMp, scaleMp, deltaMp, static_cast<mpfr_ptr>(nullptr));
    return ok;
#elif defined(USE_QD)
    qd_real zoomQD;
    if (!QDTypes::parseNumber(zoomQD, sourceZoom.c_str())) {
        return false;
    }

    const qd_real scaleQD(scaleMultiplier);
    const qd_real deltaQD = log(scaleQD) / log(qd_real(10.0));
    zoomQD -= deltaQD;
    if (zoomQD < qd_real(minSessionZoom)) {
        zoomQD = qd_real(minSessionZoom);
    }

    zoomText = QDTypes::toString(zoomQD);
    return true;
#else
    scalar_half_t sourceZoomValue = ScalarGlobals::zoom;
    if (!parseZoomInput(sourceZoom, sourceZoomValue) ||
        !std::isfinite(static_cast<double>(sourceZoomValue))) {
        return false;
    }

    const double nextZoom = std::max(
        minSessionZoom,
        static_cast<double>(sourceZoomValue) - std::log10(scaleMultiplier));
    zoomText = toScalarString(static_cast<scalar_full_t>(nextZoom));
    return true;
#endif
}

class MandelbrotBackendSession final : public Backend::Session {
public:
    MandelbrotBackendSession() {
        MPFRGlobals::initMPFR();
        QDGlobals::initQD();
        setAllDefaults();

        _zoomValue = toScalarString(zoom);
#if defined(USE_MPFR)
        MPFRGlobals::initMPFRValues(
            _pointReal.c_str(), _pointImag.c_str(), _zoomValue.c_str(),
            _seedReal.c_str(), _seedImag.c_str());
        MPFRGlobals::initMPFR(MPFRGlobals::precisionDigits());
        MPFRGlobals::initMPFRValues(
            _pointReal.c_str(), _pointImag.c_str(), _zoomValue.c_str(),
            _seedReal.c_str(), _seedImag.c_str());
#endif
#if defined(USE_KF2)
        syncKF2State();
#endif
    }

    void setCallbacks(const Backend::Callbacks &callbacks) override {
        _callbacks = callbacks;
        if (_image) _image->setCallbacks(&_callbacks);
    }

    Backend::Status getPointAtPixel(int pixelX, int pixelY,
        std::string &real, std::string &imag) override {
        return _pointFromPixelString(pixelX, pixelY, real, imag);
    }

    Backend::Status getPanPointByDelta(int deltaX, int deltaY,
        std::string &real, std::string &imag) override {
        return _panPointString(deltaX, deltaY, real, imag);
    }

    Backend::Status getZoomPointByScale(int pixelX, int pixelY,
        double scaleMultiplier,
        std::string &zoom, std::string &real, std::string &imag) override {
        return _zoomPointString(pixelX, pixelY, scaleMultiplier, zoom, real, imag);
    }

    Backend::Status getBoxZoomPoint(int left, int top, int right, int bottom,
        std::string &zoom, std::string &real, std::string &imag) override {
        return _boxZoomPointString(left, top, right, bottom, zoom, real, imag);
    }

    Backend::Status mapViewPixelToViewPixel(
        const Backend::ViewportState &sourceView,
        const Backend::ViewportState &targetView,
        int pixelX, int pixelY,
        double &mappedX, double &mappedY) override {
        return _mapViewPixelToViewPixel(sourceView, targetView,
            pixelX, pixelY, mappedX, mappedY);
    }

    int currentIterationCount() const override {
        return count;
    }

    int precisionRank() const override {
        return static_cast<int>(ScalarPrecision::precisionRank());
    }

    Backend::Status setImageSize(int imageWidth, int imageHeight, int aa) override {
        if (!setImageGlobals(imageWidth, imageHeight, aa)) {
            return makeFailure("Width, height, and aaPixels must be > 0.");
        }

        return _refreshRenderImage();
    }

    void setUseThreads(bool enabled) override {
        _useThreads = enabled;
        RenderGlobals::setUseThreads(enabled);
    }

    Backend::Status setZoom(int iterCount, const std::string &zoomScale) override {
        _autoIterations = iterCount < 1;

        if (!_setZoomValue(iterCount, zoomScale)) {
            return makeFailure("Scale must be > -3.25.");
        }

        return Backend::Status::success();
    }

    Backend::Status setPoint(const std::string &real,
        const std::string &imag) override {
        if (!isValidFullInput(real) || !isValidFullInput(imag)) {
            return makeFailure("Point coordinates must be valid numbers.");
        }

        _pointReal = real;
        _pointImag = imag;

#if defined(USE_MPFR)
        MPFRGlobals::initMPFRValues(
            _pointReal.c_str(), _pointImag.c_str(), _zoomValue.c_str(),
            _seedReal.c_str(), _seedImag.c_str());
        setZoomPoints(
            static_cast<scalar_full_t>(
                mpfr_get_d(MPFRGlobals::point_r_mp, MPFRTypes::ROUNDING)),
            static_cast<scalar_full_t>(
                mpfr_get_d(MPFRGlobals::point_i_mp, MPFRTypes::ROUNDING)),
            seed_r,
            seed_i
        );
#elif defined(USE_QD)
        QDGlobals::initQDValues(
            _pointReal.c_str(), _pointImag.c_str(), _zoomValue.c_str(),
            _seedReal.c_str(), _seedImag.c_str());
        setZoomPoints(
            static_cast<scalar_full_t>(to_double(QDGlobals::point_r_qd)),
            static_cast<scalar_full_t>(to_double(QDGlobals::point_i_qd)),
            seed_r,
            seed_i
        );
#else
        setZoomPoints(
            PARSE_F(_pointReal.c_str()),
            PARSE_F(_pointImag.c_str()),
            seed_r,
            seed_i
        );
#endif

#if defined(USE_KF2)
        syncKF2State();
#endif

        return Backend::Status::success();
    }

    Backend::Status setPoint(int pixelX, int pixelY) override {
#if defined(USE_MPFR) || defined(USE_QD)
        if (auto status = _pointFromPixelString(
            pixelX, pixelY, _pointReal, _pointImag); !status) {
            return status;
        }

        setZoomPoints(
            PARSE_F(_pointReal.c_str()),
            PARSE_F(_pointImag.c_str()),
            seed_r,
            seed_i
        );
        return Backend::Status::success();
#else
        scalar_full_t real = ZERO_F, imag = ZERO_F;

        if (auto status = _pointFromPixel(pixelX, pixelY, real, imag); !status) {
            return status;
        }

        _pointReal = toScalarString(real);
        _pointImag = toScalarString(imag);

        setZoomPoints(real, imag, seed_r, seed_i);
#if defined(USE_KF2)
        syncKF2State();
#endif
        return Backend::Status::success();
#endif
    }

    Backend::Status setSeed(const std::string &real,
        const std::string &imag) override {
        if (!isValidFullInput(real) || !isValidFullInput(imag)) {
            return makeFailure("Seed coordinates must be valid numbers.");
        }

        _seedReal = real;
        _seedImag = imag;

#if defined(USE_MPFR)
        mpfr_t seedRealMp, seedImagMp;
        mpfr_inits2(MPFRTypes::precisionBits,
            seedRealMp, seedImagMp, static_cast<mpfr_ptr>(nullptr));

        if (!MPFRTypes::parseNumber(seedRealMp, _seedReal.c_str()) ||
            !MPFRTypes::parseNumber(seedImagMp, _seedImag.c_str())) {
            mpfr_clears(seedRealMp, seedImagMp, static_cast<mpfr_ptr>(nullptr));
            return makeFailure("Seed coordinates must be valid numbers.");
        }

        setZoomPoints(
            point_r,
            point_i,
            static_cast<scalar_full_t>(mpfr_get_d(seedRealMp, MPFRTypes::ROUNDING)),
            static_cast<scalar_full_t>(mpfr_get_d(seedImagMp, MPFRTypes::ROUNDING))
        );

        mpfr_clears(seedRealMp, seedImagMp, static_cast<mpfr_ptr>(nullptr));
#elif defined(USE_QD)
        qd_real seedRealQD, seedImagQD;
        if (!QDTypes::parseNumber(seedRealQD, _seedReal.c_str()) ||
            !QDTypes::parseNumber(seedImagQD, _seedImag.c_str())) {
            return makeFailure("Seed coordinates must be valid numbers.");
        }

        setZoomPoints(
            point_r,
            point_i,
            static_cast<scalar_full_t>(to_double(seedRealQD)),
            static_cast<scalar_full_t>(to_double(seedImagQD))
        );
#else
        setZoomPoints(
            point_r,
            point_i,
            PARSE_F(_seedReal.c_str()),
            PARSE_F(_seedImag.c_str())
        );
#endif

#if defined(USE_KF2)
        syncKF2State();
#endif

        return Backend::Status::success();
    }

    Backend::Status setSeed(int pixelX, int pixelY) override {
#if defined(USE_MPFR) || defined(USE_QD)
        if (auto status = _pointFromPixelString(
            pixelX, pixelY, _seedReal, _seedImag); !status) {
            return status;
        }

        setZoomPoints(
            point_r,
            point_i,
            PARSE_F(_seedReal.c_str()),
            PARSE_F(_seedImag.c_str())
        );
        return Backend::Status::success();
#else
        scalar_full_t real = ZERO_F, imag = ZERO_F;

        if (auto status = _pointFromPixel(pixelX, pixelY, real, imag); !status) {
            return status;
        }

        _seedReal = toScalarString(real);
        _seedImag = toScalarString(imag);

        setZoomPoints(point_r, point_i, real, imag);
#if defined(USE_KF2)
        syncKF2State();
#endif
        return Backend::Status::success();
#endif
    }

    Backend::Status setFractalType(Backend::FractalType type) override {
        ScalarGlobals::setFractalType(
            static_cast<int>(type),
            isJuliaSet, isInverse
        );
#if defined(USE_KF2)
        syncKF2State();
#endif
        return Backend::Status::success();
    }

    void setFractalMode(bool juliaSet, bool inverse) override {
        ScalarGlobals::setFractalType(fractalType, juliaSet, inverse);
#if defined(USE_KF2)
        syncKF2State();
#endif
    }

    Backend::Status setFractalExponent(const std::string &exponent) override {
        if (!isValidFullInput(exponent)) {
            return makeFailure("Fractal exponent must be a valid number.");
        }

        _exponent = exponent;
        if (!ScalarGlobals::setFractalExponent(PARSE_F(_exponent.c_str()))) {
            return makeFailure("Fractal exponent must be > 1.");
        }

#if defined(USE_KF2)
        syncKF2State();
#endif
        return Backend::Status::success();
    }

    Backend::Status setColorMethod(Backend::ColorMethod method) override {
        ScalarGlobals::setColorMethod(static_cast<int>(method));
#if defined(USE_KF2)
        syncKF2State();
#endif
        return Backend::Status::success();
    }

    Backend::Status setColorFormula(float freqR, float freqG, float freqB,
        float freqMult) override {
        return setSinePalette({
            .freqR = freqR,
            .freqG = freqG,
            .freqB = freqB,
            .freqMult = freqMult
            });
    }

    Backend::Status setSinePalette(
        const Backend::SinePaletteConfig &paletteConfig
    ) override {
        if (!setSinePaletteGlobals(
            paletteConfig.freqR,
            paletteConfig.freqG,
            paletteConfig.freqB,
            paletteConfig.freqMult
        )) {
            return makeFailure("Frequency multiplier must be non-zero.");
        }

        return Backend::Status::success();
    }

    Backend::Status setColorPalette(
        const Backend::PaletteRGBConfig &paletteConfig
    ) override {
        return _setPaletteImpl(paletteConfig,
            "Palette entries must use valid RGB values and lengths >= 0.");
    }

    Backend::Status setColorPalette(
        const Backend::PaletteHexConfig &paletteConfig
    ) override {
        return _setPaletteImpl(paletteConfig,
            "Palette entries must use valid #RRGGBB colors and lengths >= 0.");
    }

    Backend::Status setLightColor(const Backend::LightColor &color) override {
        const ScalarColor scalarColor{
            CAST_H(color.R),
            CAST_H(color.G),
            CAST_H(color.B)
        };

        if (!setLightGlobals(
            light_r * light_h,
            light_i * light_h,
            scalarColor
        )) {
            return makeFailure(
                "Light color components must be between 0 and 1.");
        }

        return Backend::Status::success();
    }

    Backend::Status setLightColor(const std::string &colorHex) override {
        const ScalarColor color = ScalarColor::fromString(colorHex);
        if (color.R < ZERO_H || color.G < ZERO_H || color.B < ZERO_H) {
            return makeFailure("Light color must be a valid hex color.");
        }

        return setLightColor(Backend::LightColor{
            static_cast<float>(color.R),
            static_cast<float>(color.G),
            static_cast<float>(color.B)
            });
    }

    Backend::Status setLight(float real, float imag) override {
        if (!setLightGlobals(real, imag, lightColor)) {
            return makeFailure("Light vector must be non-zero.");
        }

        return Backend::Status::success();
    }

    Backend::Status setLight(int pixelX, int pixelY) override {
        scalar_full_t real = ZERO_F, imag = ZERO_F;

        if (auto status = _pointFromPixel(pixelX, pixelY, real, imag); !status) {
            return status;
        }

        return setLight(static_cast<float>(real), static_cast<float>(imag));
    }

    Backend::ImageView renderPalettePreview(int previewWidth,
        int previewHeight) override {
        if (previewWidth <= 0 || previewHeight <= 0) return {};

        const bool needsNewPreview = !_palettePreviewImage ||
            _palettePreviewImage->outputWidth() != previewWidth ||
            _palettePreviewImage->outputHeight() != previewHeight;

        if (needsNewPreview) {
            _palettePreviewImage = Image::create(
                previewWidth, previewHeight, false, 1);
            if (!_palettePreviewImage) return {};
        } else {
            _palettePreviewImage->clear();
        }

        const scalar_half_t totalLength =
            palette.getTotalLength() > ZERO_H ?
            palette.getTotalLength() : ONE_H;
        const scalar_half_t widthDenom =
            previewWidth > 1 ? CAST_H(previewWidth - 1) : ONE_H;

        for (int x = 0; x < previewWidth; ++x) {
            const scalar_half_t pos = previewWidth > 1 ?
                totalLength * CAST_H(x) / widthDenom :
                ZERO_H;

            const ScalarColor sample = palette.sample(pos);

            const uint8_t r = toColorByte(sample.R);
            const uint8_t g = toColorByte(sample.G);
            const uint8_t b = toColorByte(sample.B);

            for (int y = 0; y < previewHeight; ++y) {
                const size_t idx =
                    static_cast<size_t>(y) * _palettePreviewImage->strideWidth() +
                    x * Image::STRIDE;
                _palettePreviewImage->pixels()[idx] = r;
                _palettePreviewImage->pixels()[idx + 1] = g;
                _palettePreviewImage->pixels()[idx + 2] = b;
            }
        }

        return makeImageView(_palettePreviewImage.get());
    }

    Backend::ImageView renderSinePreview(int previewWidth,
        int previewHeight, float rangeMin, float rangeMax) override {
        if (previewWidth <= 0 || previewHeight <= 0 || rangeMax <= rangeMin) {
            return {};
        }

        const bool needsNewPreview = !_sinePreviewImage ||
            _sinePreviewImage->outputWidth() != previewWidth ||
            _sinePreviewImage->outputHeight() != previewHeight;

        if (needsNewPreview) {
            _sinePreviewImage = Image::create(
                previewWidth, previewHeight, false, 1);
            if (!_sinePreviewImage) return {};
        } else {
            _sinePreviewImage->clear();
        }

        const scalar_half_t minValue = CAST_H(rangeMin);
        const scalar_half_t widthDenom =
            previewWidth > 1 ? CAST_H(previewWidth - 1) : ONE_H;
        const scalar_half_t span = CAST_H(rangeMax - rangeMin);

        for (int x = 0; x < previewWidth; ++x) {
            const scalar_half_t value = previewWidth > 1 ?
                minValue + span * CAST_H(x) / widthDenom :
                minValue;

            scalar_half_t r = ZERO_H, g = ZERO_H, b = ZERO_H;
            ScalarRenderer::sampleColorFormula(value, r, g, b);

            for (int y = 0; y < previewHeight; ++y) {
                size_t pos =
                    static_cast<size_t>(y) * _sinePreviewImage->strideWidth() +
                    x * Image::STRIDE;
                ScalarRenderer::setPixel(_sinePreviewImage->pixels(), pos, r, g, b);
            }
        }

        return makeImageView(_sinePreviewImage.get());
    }

    Backend::Status render() override {
        if (!_image) {
            return makeFailure("Image size must be set before rendering.");
        }

        _image->clear();

        VectorGlobals::initVectors();
        MPFRGlobals::initMPFRValues(
            _pointReal.c_str(), _pointImag.c_str(), _zoomValue.c_str(),
            _seedReal.c_str(), _seedImag.c_str());
        MPFRGlobals::initMPFR(MPFRGlobals::precisionDigits());
        MPFRGlobals::initMPFRValues(
            _pointReal.c_str(), _pointImag.c_str(), _zoomValue.c_str(),
            _seedReal.c_str(), _seedImag.c_str());
        QDGlobals::initQDValues(
            _pointReal.c_str(), _pointImag.c_str(), _zoomValue.c_str(),
            _seedReal.c_str(), _seedImag.c_str());

#if defined(USE_KF2)
        syncKF2State();
#endif

        RenderIterationStats iterStats;
        renderImage(_image.get(), &_callbacks,
            static_cast<bool>(_callbacks.onProgress),
            static_cast<bool>(_callbacks.onInfo) || _autoIterations,
            _autoIterations
            ? OptionalIterationStats(std::ref(iterStats))
            : std::nullopt);

        if (_autoIterations) {
            applyAutoIterations(iterStats);
        }

        return Backend::Status::success();
    }

    void clearImage() override {
        if (_image) _image->clear();
    }

    void forceKill() override {
#if defined(USE_KF2)
        KF2Renderer::forceStop();
#endif
        forceKillRenderThreads();
    }

    Backend::ImageView imageView() const override {
        return makeImageView(_image.get());
    }

    Backend::Status saveImage(const std::string &path, bool appendDate,
        const std::string &type) override {
        if (!_image) {
            return makeFailure("No rendered image is available.");
        }

        if (!_image->saveToFile(path, appendDate, type)) {
            return makeFailure("Failed to save image.");
        }

        return Backend::Status::success();
    }

private:
    Backend::Callbacks _callbacks;
    bool _autoIterations = true;
    bool _useThreads = false;

    std::string _pointReal = "0";
    std::string _pointImag = "0";
    std::string _seedReal = "0";
    std::string _seedImag = "0";
    std::string _zoomValue = "0";
    std::string _exponent = "2";

    std::unique_ptr<Image> _image;
    std::unique_ptr<Image> _palettePreviewImage;
    std::unique_ptr<Image> _sinePreviewImage;
    int _lastAAPixels = 1;

#if defined(USE_KF2)
    void syncKF2State() {
        KF2Globals::syncCurrentValues(
            _pointReal,
            _pointImag,
            _zoomValue,
            _seedReal,
            _seedImag,
            _exponent,
            count,
            fractalType,
            colorMethod,
            isJuliaSet,
            isInverse
        );
    }
#endif

    void applyAutoIterations(const RenderIterationStats &stats) {
        if (!stats.valid) return;

        _setZoomValue(stats.autoIterationCount(count), _zoomValue);
    }

    bool _setZoomValue(int iterCount, const std::string &zoomText) {
        if (!isZoomAboveMinimum(zoomText)) {
            return false;
        }

        scalar_half_t zoomScale = zoom;
        if (!parseZoomInput(zoomText, zoomScale)) {
            return false;
        }

        if (!std::isfinite(static_cast<double>(zoomScale))) {
            zoomScale = zoom;
        }

        if (!setZoomGlobals(iterCount, zoomScale)) {
            return false;
        }

        _zoomValue = zoomText;

#if defined(USE_MPFR)
        MPFRGlobals::initMPFRValues(
            _pointReal.c_str(), _pointImag.c_str(), _zoomValue.c_str(),
            _seedReal.c_str(), _seedImag.c_str());
        MPFRGlobals::initMPFR(MPFRGlobals::precisionDigits());
        MPFRGlobals::initMPFRValues(
            _pointReal.c_str(), _pointImag.c_str(), _zoomValue.c_str(),
            _seedReal.c_str(), _seedImag.c_str());
#endif

#if defined(USE_KF2)
        syncKF2State();
#endif

        return true;
    }

    Backend::Status _refreshRenderImage() {
        if (outputWidth <= 0 || outputHeight <= 0 || aaPixels <= 0) {
            _image.reset();
            _lastAAPixels = 1;
            return Backend::Status::success();
        }

        const bool needsNewImage = !_image ||
            _image->outputWidth() != outputWidth ||
            _image->outputHeight() != outputHeight ||
            _lastAAPixels != aaPixels;

        if (needsNewImage) {
            if (auto image = Image::create(outputWidth, outputHeight,
                true, aaPixels); !image) {
                return makeFailure("Failed to allocate image.");
            } else {
                _image = std::move(image);
            }

            _image->setCallbacks(&_callbacks);
            _lastAAPixels = aaPixels;
        }

        return Backend::Status::success();
    }

    Backend::Status _pointFromPixel(int pixelX, int pixelY,
        scalar_full_t &real, scalar_full_t &imag) const {
        if (outputWidth <= 0 || outputHeight <= 0) {
            return makeFailure("Image size must be set before using pixel coordinates.");
        }

        getOutputCenterPoint(pixelX, pixelY, real, imag);
        return Backend::Status::success();
    }

    Backend::Status _panPoint(scalar_full_t deltaX, scalar_full_t deltaY,
        scalar_full_t &real, scalar_full_t &imag) const {
        if (outputWidth <= 0 || outputHeight <= 0) {
            return makeFailure("Image size must be set before using pixel coordinates.");
        }

        getPanCenterPoint(static_cast<int>(deltaX), static_cast<int>(deltaY), real, imag);
        return Backend::Status::success();
    }

    Backend::Status _pointFromPixelString(int pixelX, int pixelY,
        std::string &real, std::string &imag) const {
#if defined(USE_MPFR)
        if (outputWidth <= 0 || outputHeight <= 0) {
            return makeFailure("Image size must be set before using pixel coordinates.");
        }

        MPFRGlobals::initMPFRValues(
            _pointReal.c_str(), _pointImag.c_str(), _zoomValue.c_str(),
            _seedReal.c_str(), _seedImag.c_str());

        mpfr_t realMp, imagMp;
        mpfr_inits2(MPFRTypes::precisionBits,
            realMp, imagMp, static_cast<mpfr_ptr>(nullptr));

        getOutputCenterPoint_mp(realMp, imagMp, pixelX, pixelY);

        real = MPFRTypes::toString(realMp);
        imag = MPFRTypes::toString(imagMp);

        mpfr_clears(realMp, imagMp, static_cast<mpfr_ptr>(nullptr));
        return Backend::Status::success();
#elif defined(USE_QD)
        if (outputWidth <= 0 || outputHeight <= 0) {
            return makeFailure("Image size must be set before using pixel coordinates.");
        }

        QDGlobals::initQDValues(
            _pointReal.c_str(), _pointImag.c_str(), _zoomValue.c_str(),
            _seedReal.c_str(), _seedImag.c_str());

        qd_real realQD, imagQD;
        getOutputCenterPoint_qd(realQD, imagQD, pixelX, pixelY);

        real = QDTypes::toString(realQD);
        imag = QDTypes::toString(imagQD);
        return Backend::Status::success();
#else
        scalar_full_t realVal = ZERO_F, imagVal = ZERO_F;
        if (auto status = _pointFromPixel(pixelX, pixelY, realVal, imagVal); !status) {
            return status;
        }

        real = toScalarString(realVal);
        imag = toScalarString(imagVal);
        return Backend::Status::success();
#endif
    }

    Backend::Status _panPointString(int deltaX, int deltaY,
        std::string &real, std::string &imag) const {
#if defined(USE_MPFR)
        if (outputWidth <= 0 || outputHeight <= 0) {
            return makeFailure("Image size must be set before using pixel coordinates.");
        }

        MPFRGlobals::initMPFRValues(
            _pointReal.c_str(), _pointImag.c_str(), _zoomValue.c_str(),
            _seedReal.c_str(), _seedImag.c_str());

        mpfr_t realMp, imagMp;
        mpfr_inits2(MPFRTypes::precisionBits,
            realMp, imagMp, static_cast<mpfr_ptr>(nullptr));

        getPanCenterPoint_mp(realMp, imagMp, deltaX, deltaY);

        real = MPFRTypes::toString(realMp);
        imag = MPFRTypes::toString(imagMp);

        mpfr_clears(realMp, imagMp, static_cast<mpfr_ptr>(nullptr));
        return Backend::Status::success();
#elif defined(USE_QD)
        if (outputWidth <= 0 || outputHeight <= 0) {
            return makeFailure("Image size must be set before using pixel coordinates.");
        }

        QDGlobals::initQDValues(
            _pointReal.c_str(), _pointImag.c_str(), _zoomValue.c_str(),
            _seedReal.c_str(), _seedImag.c_str());

        qd_real realQD, imagQD;
        getPanCenterPoint_qd(realQD, imagQD, deltaX, deltaY);

        real = QDTypes::toString(realQD);
        imag = QDTypes::toString(imagQD);
        return Backend::Status::success();
#else
        scalar_full_t realVal = ZERO_F, imagVal = ZERO_F;
        if (auto status = _panPoint(deltaX, deltaY, realVal, imagVal); !status) {
            return status;
        }

        real = toScalarString(realVal);
        imag = toScalarString(imagVal);
        return Backend::Status::success();
#endif
    }

    Backend::Status _zoomPointForTargetZoom(int pixelX, int pixelY,
        const std::string &targetZoom,
        std::string &real, std::string &imag) {
        if (!isZoomAboveMinimum(targetZoom)) {
            return makeFailure("Scale must be > -3.25.");
        }

#if defined(USE_MPFR)
        std::string targetReal, targetImag;
        if (auto status = _pointFromPixelString(
            pixelX, pixelY, targetReal, targetImag); !status) {
            return status;
        }

        const int oldCount = count;
        const std::string oldZoomValue = _zoomValue;
        const std::string oldPointReal = _pointReal;
        const std::string oldPointImag = _pointImag;

        if (!_setZoomValue(oldCount, targetZoom)) {
            return makeFailure("Scale must be > -3.25.");
        }

        _pointReal = "0";
        _pointImag = "0";

        std::string offsetReal, offsetImag;
        const Backend::Status offsetStatus = _pointFromPixelString(
            pixelX, pixelY, offsetReal, offsetImag);

        _pointReal = oldPointReal;
        _pointImag = oldPointImag;
        _setZoomValue(oldCount, oldZoomValue);

        if (!offsetStatus) return offsetStatus;

        mpfr_t targetRealMp, targetImagMp, offsetRealMp, offsetImagMp;
        mpfr_t pointRealMp, pointImagMp;
        mpfr_inits2(MPFRTypes::precisionBits,
            targetRealMp, targetImagMp, offsetRealMp, offsetImagMp,
            pointRealMp, pointImagMp, static_cast<mpfr_ptr>(nullptr));

        if (!MPFRTypes::parseNumber(targetRealMp, targetReal.c_str()) ||
            !MPFRTypes::parseNumber(targetImagMp, targetImag.c_str()) ||
            !MPFRTypes::parseNumber(offsetRealMp, offsetReal.c_str()) ||
            !MPFRTypes::parseNumber(offsetImagMp, offsetImag.c_str())) {
            mpfr_clears(targetRealMp, targetImagMp, offsetRealMp, offsetImagMp,
                pointRealMp, pointImagMp, static_cast<mpfr_ptr>(nullptr));
            return makeFailure("Point coordinates must be valid numbers.");
        }

        mpfr_sub(pointRealMp, targetRealMp, offsetRealMp, MPFRTypes::ROUNDING);
        mpfr_sub(pointImagMp, offsetImagMp, targetImagMp, MPFRTypes::ROUNDING);

        real = MPFRTypes::toString(pointRealMp);
        imag = MPFRTypes::toString(pointImagMp);

        mpfr_clears(targetRealMp, targetImagMp, offsetRealMp, offsetImagMp,
            pointRealMp, pointImagMp, static_cast<mpfr_ptr>(nullptr));
        return Backend::Status::success();
#elif defined(USE_QD)
        std::string targetReal, targetImag;
        if (auto status = _pointFromPixelString(
            pixelX, pixelY, targetReal, targetImag); !status) {
            return status;
        }

        const int oldCount = count;
        const std::string oldZoomValue = _zoomValue;
        const std::string oldPointReal = _pointReal;
        const std::string oldPointImag = _pointImag;

        if (!_setZoomValue(oldCount, targetZoom)) {
            return makeFailure("Scale must be > -3.25.");
        }

        _pointReal = "0";
        _pointImag = "0";

        std::string offsetReal, offsetImag;
        const Backend::Status offsetStatus = _pointFromPixelString(
            pixelX, pixelY, offsetReal, offsetImag);

        _pointReal = oldPointReal;
        _pointImag = oldPointImag;
        _setZoomValue(oldCount, oldZoomValue);

        if (!offsetStatus) return offsetStatus;

        qd_real targetRealQD, targetImagQD;
        qd_real offsetRealQD, offsetImagQD;
        qd_real pointRealQD, pointImagQD;

        if (!QDTypes::parseNumber(targetRealQD, targetReal.c_str()) ||
            !QDTypes::parseNumber(targetImagQD, targetImag.c_str()) ||
            !QDTypes::parseNumber(offsetRealQD, offsetReal.c_str()) ||
            !QDTypes::parseNumber(offsetImagQD, offsetImag.c_str())) {
            return makeFailure("Point coordinates must be valid numbers.");
        }

        pointRealQD = targetRealQD - offsetRealQD;
        pointImagQD = offsetImagQD - targetImagQD;

        real = QDTypes::toString(pointRealQD);
        imag = QDTypes::toString(pointImagQD);
        return Backend::Status::success();
#else
        scalar_full_t targetReal = ZERO_F;
        scalar_full_t targetImag = ZERO_F;
        if (auto status = _pointFromPixel(pixelX, pixelY, targetReal, targetImag); !status) {
            return status;
        }

        const int oldCount = count;
        const std::string oldZoomValue = _zoomValue;
        const scalar_full_t oldPointR = point_r;
        const scalar_full_t oldPointI = point_i;
        const scalar_full_t oldSeedR = seed_r;
        const scalar_full_t oldSeedI = seed_i;

        if (!_setZoomValue(oldCount, targetZoom)) {
            return makeFailure("Scale must be > -3.25.");
        }

        setZoomPoints(ZERO_F, ZERO_F, oldSeedR, oldSeedI);
        scalar_full_t offsetReal = ZERO_F;
        scalar_full_t offsetImag = ZERO_F;
        const Backend::Status offsetStatus = _pointFromPixel(
            pixelX, pixelY, offsetReal, offsetImag);

        _setZoomValue(oldCount, oldZoomValue);
        setZoomPoints(oldPointR, oldPointI, oldSeedR, oldSeedI);

        if (!offsetStatus) return offsetStatus;

        real = toScalarString(targetReal - offsetReal);
        imag = toScalarString(offsetImag - targetImag);
        return Backend::Status::success();
#endif
    }

    Backend::Status _zoomPointString(int pixelX, int pixelY,
        double scaleMultiplier,
        std::string &zoom, std::string &real, std::string &imag) {
        if (!scaleZoomText(_zoomValue, scaleMultiplier, zoom)) {
            return makeFailure("Scale multiplier must be > 0.");
        }

        return _zoomPointForTargetZoom(pixelX, pixelY, zoom, real, imag);
    }

    Backend::Status _boxZoomPointString(int left, int top, int right, int bottom,
        std::string &zoom, std::string &real, std::string &imag) {
        if (outputWidth <= 0 || outputHeight <= 0) {
            return makeFailure("Image size must be set before using pixel coordinates.");
        }

        const int clampedLeft = std::clamp(std::min(left, right), 0,
            std::max(0, outputWidth - 1));
        const int clampedRight = std::clamp(std::max(left, right), 0,
            std::max(0, outputWidth - 1));
        const int clampedTop = std::clamp(std::min(top, bottom), 0,
            std::max(0, outputHeight - 1));
        const int clampedBottom = std::clamp(std::max(top, bottom), 0,
            std::max(0, outputHeight - 1));

        const int selectionWidth = clampedRight - clampedLeft + 1;
        const int selectionHeight = clampedBottom - clampedTop + 1;
        if (selectionWidth <= 0 || selectionHeight <= 0) {
            return makeFailure("Selection must cover at least one pixel.");
        }

        const double xScale = static_cast<double>(outputWidth) / selectionWidth;
        const double yScale = static_cast<double>(outputHeight) / selectionHeight;
        const double fitScale = std::min(xScale, yScale);
        if (!(fitScale > 0.0) || !std::isfinite(fitScale)) {
            return makeFailure("Selection must have non-zero size.");
        }
        if (!scaleZoomText(_zoomValue, 1.0 / fitScale, zoom)) {
            return makeFailure("Scale multiplier must be > 0.");
        }

#if defined(USE_MPFR)
        MPFRGlobals::initMPFRValues(
            _pointReal.c_str(), _pointImag.c_str(), _zoomValue.c_str(),
            _seedReal.c_str(), _seedImag.c_str());

        mpfr_t centerRealMp, centerImagMp;
        mpfr_inits2(MPFRTypes::precisionBits,
            centerRealMp, centerImagMp, static_cast<mpfr_ptr>(nullptr));

        getBoxCenterPoint_mp(centerRealMp, centerImagMp,
            clampedLeft, clampedTop, clampedRight, clampedBottom);

        real = MPFRTypes::toString(centerRealMp);
        imag = MPFRTypes::toString(centerImagMp);

        mpfr_clears(centerRealMp, centerImagMp, static_cast<mpfr_ptr>(nullptr));
        return Backend::Status::success();
#elif defined(USE_QD)
        QDGlobals::initQDValues(
            _pointReal.c_str(), _pointImag.c_str(), _zoomValue.c_str(),
            _seedReal.c_str(), _seedImag.c_str());

        qd_real centerReal;
        qd_real centerImag;
        getBoxCenterPoint_qd(centerReal, centerImag,
            clampedLeft, clampedTop, clampedRight, clampedBottom);

        real = QDTypes::toString(centerReal);
        imag = QDTypes::toString(centerImag);
        return Backend::Status::success();
#else
        scalar_full_t centerReal = ZERO_F;
        scalar_full_t centerImag = ZERO_F;
        getBoxCenterPoint(clampedLeft, clampedTop, clampedRight, clampedBottom,
            centerReal, centerImag);
        real = toScalarString(centerReal);
        imag = toScalarString(centerImag);
        return Backend::Status::success();
#endif
    }

    struct SavedNavigationState {
        int iterations = 0;
        int width = 0;
        int height = 0;
        int aa = 1;
        std::string pointReal = "0";
        std::string pointImag = "0";
        std::string zoom = "0";
    };

    SavedNavigationState _captureNavigationState() const {
        return {
            .iterations = count,
            .width = outputWidth,
            .height = outputHeight,
            .aa = aaPixels,
            .pointReal = _pointReal,
            .pointImag = _pointImag,
            .zoom = _zoomValue
        };
    }

    Backend::Status _restoreNavigationState(const SavedNavigationState &state) {
        if (state.width <= 0 || state.height <= 0) {
            return Backend::Status::success();
        }

        if (auto status = setImageSize(state.width, state.height, state.aa); !status) {
            return status;
        }

        if (auto status = setZoom(state.iterations, state.zoom); !status) {
            return status;
        }
        return setPoint(state.pointReal, state.pointImag);
    }

    Backend::Status _applyViewportState(const Backend::ViewportState &view) {
        if (view.outputWidth <= 0 || view.outputHeight <= 0) {
            return makeFailure("Image size must be set before using pixel coordinates.");
        }

        if (auto status = setImageSize(
            view.outputWidth, view.outputHeight, std::max(1, aaPixels)); !status) {
            return status;
        }
        if (auto status = setZoom(count, view.zoom); !status) {
            return status;
        }
        return setPoint(view.pointReal, view.pointImag);
    }

    Backend::Status _pixelFromCurrentPointString(
        const std::string &real,
        const std::string &imag,
        double &pixelX,
        double &pixelY) const {
#if defined(USE_MPFR)
        MPFRGlobals::initMPFRValues(
            _pointReal.c_str(), _pointImag.c_str(), _zoomValue.c_str(),
            _seedReal.c_str(), _seedImag.c_str());

        mpfr_t realMp, imagMp, xMp, yMp;
        mpfr_inits2(MPFRTypes::precisionBits,
            realMp, imagMp, xMp, yMp, static_cast<mpfr_ptr>(nullptr));

        if (!MPFRTypes::parseNumber(realMp, real.c_str()) ||
            !MPFRTypes::parseNumber(imagMp, imag.c_str())) {
            mpfr_clears(realMp, imagMp, xMp, yMp, static_cast<mpfr_ptr>(nullptr));
            return makeFailure("Point coordinates must be valid numbers.");
        }

        getOutputPixelPoint_mp(xMp, yMp, realMp, imagMp);
        pixelX = mpfr_get_d(xMp, MPFRTypes::ROUNDING);
        pixelY = mpfr_get_d(yMp, MPFRTypes::ROUNDING);

        mpfr_clears(realMp, imagMp, xMp, yMp, static_cast<mpfr_ptr>(nullptr));
        return Backend::Status::success();
#elif defined(USE_QD)
        QDGlobals::initQDValues(
            _pointReal.c_str(), _pointImag.c_str(), _zoomValue.c_str(),
            _seedReal.c_str(), _seedImag.c_str());

        qd_real realQD, imagQD, xQD, yQD;
        if (!QDTypes::parseNumber(realQD, real.c_str()) ||
            !QDTypes::parseNumber(imagQD, imag.c_str())) {
            return makeFailure("Point coordinates must be valid numbers.");
        }

        getOutputPixelPoint_qd(xQD, yQD, realQD, imagQD);
        pixelX = to_double(xQD);
        pixelY = to_double(yQD);
        return Backend::Status::success();
#else
        bool okReal = false;
        bool okImag = false;
        const scalar_full_t realVal = parseNumber<scalar_full_t>(real, std::ref(okReal));
        const scalar_full_t imagVal = parseNumber<scalar_full_t>(imag, std::ref(okImag));
        if (!okReal || !okImag) {
            return makeFailure("Point coordinates must be valid numbers.");
        }

        scalar_full_t xVal = ZERO_F;
        scalar_full_t yVal = ZERO_F;
        getOutputPixelPoint(realVal, imagVal, xVal, yVal);
        pixelX = static_cast<double>(xVal);
        pixelY = static_cast<double>(yVal);
        return Backend::Status::success();
#endif
    }

    Backend::Status _mapViewPixelToViewPixel(
        const Backend::ViewportState &sourceView,
        const Backend::ViewportState &targetView,
        int pixelX, int pixelY,
        double &mappedX, double &mappedY) {
        const SavedNavigationState saved = _captureNavigationState();

        const auto restore = [&]() {
            return _restoreNavigationState(saved);
            };

        Backend::Status status = _applyViewportState(targetView);
        if (!status) {
            restore();
            return status;
        }

        std::string real;
        std::string imag;
        status = _pointFromPixelString(pixelX, pixelY, real, imag);
        if (!status) {
            restore();
            return status;
        }

        status = _applyViewportState(sourceView);
        if (!status) {
            restore();
            return status;
        }

        status = _pixelFromCurrentPointString(real, imag, mappedX, mappedY);
        const Backend::Status restoreStatus = restore();
        if (!status) {
            return status;
        }
        return restoreStatus;
    }

    template<typename PaletteConfigT>
    Backend::Status _setPaletteImpl(
        const PaletteConfigT &paletteConfig,
        const std::string &invalidEntryMessage
    ) {
        if (paletteConfig.totalLength <= ZERO_H || paletteConfig.offset < ZERO_H) {
            return makeFailure("Palette length must be > 0 and offset must be >= 0.");
        }

        std::vector<ScalarPaletteColor> entries;
        entries.reserve(paletteConfig.entries.size());

        for (const auto &entry : paletteConfig.entries) {
            ScalarPaletteColor parsed = makePaletteColor(entry);
            if (!parsed.valid()) {
                return makeFailure(invalidEntryMessage);
            }

            entries.push_back(parsed);
        }

        if (!setPaletteGlobals(entries, paletteConfig.totalLength,
            paletteConfig.offset, paletteConfig.blendEnds)) {
            return makeFailure("Palette must contain at least 2 valid entries.");
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