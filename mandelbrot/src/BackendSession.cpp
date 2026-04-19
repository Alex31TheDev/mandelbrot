#include "BackendAPI.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>

#include "util/ParserUtil.h"
#include "image/Image.h"
#include "render/RenderGlobals.h"
#include "render/RenderImage.h"
#include "scalar/ScalarGlobals.h"
#include "scalar/ScalarColors.h"
#include "scalar/ScalarCoords.h"
#include "scalar/ScalarRenderer.h"
#include "vector/VectorGlobals.h"
#include "mpfr/MPFRGlobals.h"
#include "mpfr/MPFRCoords.h"
#include "qd/QDGlobals.h"
#include "qd/QDCoords.h"
#include "qd/QDTypes.h"

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

static bool isValidPaletteColor(const ScalarPaletteColor &entry) {
    return entry.R >= ZERO_H && entry.G >= ZERO_H &&
        entry.B >= ZERO_H && entry.length >= ZERO_H;
}

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

static uint8_t toColorByte(scalar_half_t value) {
    const double clamped = std::clamp(static_cast<double>(value), 0.0, 1.0);
    return static_cast<uint8_t>(clamped * 255.0 + 0.5);
}

template<typename T>
static std::string toScalarString(const T &value) {
    std::ostringstream oss;

    if constexpr (std::is_floating_point_v<T>) {
        oss << std::setprecision(std::numeric_limits<T>::max_digits10);
    }

    oss << value;
    return oss.str();
}

#if defined(USE_MPFR)
static std::string toMpfrString(mpfr_srcptr value) {
    const int digits = std::max(24,
        static_cast<int>(std::ceil(
            static_cast<double>(mpfr_get_prec(value)) * 0.30103)) + 4);

    char *buffer = nullptr;
    if (mpfr_asprintf(&buffer, "%.*Rg", digits, value) < 0 || !buffer) {
        return "0";
    }

    std::string out(buffer);
    mpfr_free_str(buffer);
    return out;
}
#endif

#if defined(USE_QD)
static std::string toQdString(const qd_real &value) {
    if (!isfinite(value)) return "0";
    if (value == qd_real(0.0)) return "0";

    qd_real v = value;
    bool neg = false;
    if (v < qd_real(0.0)) {
        neg = true;
        v = -v;
    }

    int exp10 = static_cast<int>(
        std::floor(std::log10(std::abs(to_double(v))))
        );

    qd_real scaled = v / pow(qd_real(10.0), exp10);
    while (scaled >= qd_real(10.0)) {
        scaled /= qd_real(10.0);
        ++exp10;
    }
    while (scaled < qd_real(1.0)) {
        scaled *= qd_real(10.0);
        --exp10;
    }

    constexpr int DIGITS = 62;
    std::string digits;
    digits.reserve(DIGITS);

    for (int i = 0; i < DIGITS; ++i) {
        int d = static_cast<int>(std::floor(to_double(scaled)));
        d = std::clamp(d, 0, 9);

        digits.push_back(static_cast<char>('0' + d));
        scaled = (scaled - qd_real(static_cast<double>(d))) * qd_real(10.0);
    }

    std::string out;
    out.reserve(DIGITS + 16);

    if (neg) out.push_back('-');
    out.push_back(digits[0]);
    out.push_back('.');
    out.append(digits.begin() + 1, digits.end());

    while (!out.empty() && out.back() == '0') {
        out.pop_back();
    }
    if (!out.empty() && out.back() == '.') {
        out.push_back('0');
    }

    out.push_back('e');
    if (exp10 >= 0) out.push_back('+');
    out.append(std::to_string(exp10));
    return out;
}
#endif

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

class MandelbrotBackendSession final : public Backend::Session {
public:
    MandelbrotBackendSession() {
        MPFRGlobals::initMPFR();
        QDGlobals::initQD();
        setAllDefaults();
    }

    void setCallbacks(const Backend::Callbacks &callbacks) override {
        _callbacks = callbacks;
        if (_image) _image->setCallbacks(&_callbacks);
    }

    Backend::Status getPointAtPixel(int pixelX, int pixelY,
        std::string &real, std::string &imag) override {
        return _pointFromPixelString(pixelX, pixelY, real, imag);
    }

    Backend::Status computeZoomPointForPixel(int pixelX, int pixelY,
        float targetZoom, std::string &real, std::string &imag) override {
        if (!std::isfinite(targetZoom) || targetZoom <= -3.25f) {
            return makeFailure("Scale must be > -3.25.");
        }

#if defined(USE_MPFR)
        std::string targetReal;
        std::string targetImag;
        if (auto status = _pointFromPixelString(
            pixelX, pixelY, targetReal, targetImag); !status) {
            return status;
        }

        const int oldCount = count;
        const scalar_half_t oldZoom = zoom;
        const std::string oldPointReal = _pointReal;
        const std::string oldPointImag = _pointImag;

        if (!setZoomGlobals(oldCount, targetZoom)) {
            return makeFailure("Scale must be > -3.25.");
        }

        _pointReal = "0";
        _pointImag = "0";

        std::string offsetReal;
        std::string offsetImag;
        const Backend::Status offsetStatus = _pointFromPixelString(
            pixelX, pixelY, offsetReal, offsetImag);

        _pointReal = oldPointReal;
        _pointImag = oldPointImag;
        setZoomGlobals(oldCount, oldZoom);

        if (!offsetStatus) return offsetStatus;

        mpfr_t targetRealMp, targetImagMp, offsetRealMp, offsetImagMp;
        mpfr_t pointRealMp, pointImagMp;
        mpfr_inits2(MPFRTypes::getPrecisionBits(),
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

        real = toMpfrString(pointRealMp);
        imag = toMpfrString(pointImagMp);

        mpfr_clears(targetRealMp, targetImagMp, offsetRealMp, offsetImagMp,
            pointRealMp, pointImagMp, static_cast<mpfr_ptr>(nullptr));
        return Backend::Status::success();
#elif defined(USE_QD)
        std::string targetReal;
        std::string targetImag;
        if (auto status = _pointFromPixelString(
            pixelX, pixelY, targetReal, targetImag); !status) {
            return status;
        }

        const int oldCount = count;
        const scalar_half_t oldZoom = zoom;
        const std::string oldPointReal = _pointReal;
        const std::string oldPointImag = _pointImag;

        if (!setZoomGlobals(oldCount, targetZoom)) {
            return makeFailure("Scale must be > -3.25.");
        }

        _pointReal = "0";
        _pointImag = "0";

        std::string offsetReal;
        std::string offsetImag;
        const Backend::Status offsetStatus = _pointFromPixelString(
            pixelX, pixelY, offsetReal, offsetImag);

        _pointReal = oldPointReal;
        _pointImag = oldPointImag;
        setZoomGlobals(oldCount, oldZoom);

        if (!offsetStatus) return offsetStatus;

        qd_real targetRealQd;
        qd_real targetImagQd;
        qd_real offsetRealQd;
        qd_real offsetImagQd;
        qd_real pointRealQd;
        qd_real pointImagQd;

        if (!QDTypes::parseNumber(targetRealQd, targetReal.c_str()) ||
            !QDTypes::parseNumber(targetImagQd, targetImag.c_str()) ||
            !QDTypes::parseNumber(offsetRealQd, offsetReal.c_str()) ||
            !QDTypes::parseNumber(offsetImagQd, offsetImag.c_str())) {
            return makeFailure("Point coordinates must be valid numbers.");
        }

        pointRealQd = targetRealQd - offsetRealQd;
        pointImagQd = offsetImagQd - targetImagQd;

        real = toQdString(pointRealQd);
        imag = toQdString(pointImagQd);

        return Backend::Status::success();
#else
        scalar_full_t targetReal = ZERO_F;
        scalar_full_t targetImag = ZERO_F;
        if (auto status = _pointFromPixel(pixelX, pixelY, targetReal, targetImag); !status) {
            return status;
        }

        const int oldCount = count;
        const scalar_half_t oldZoom = zoom;
        const scalar_full_t oldPointR = point_r;
        const scalar_full_t oldPointI = point_i;
        const scalar_full_t oldSeedR = seed_r;
        const scalar_full_t oldSeedI = seed_i;

        if (!setZoomGlobals(oldCount, targetZoom)) {
            return makeFailure("Scale must be > -3.25.");
        }

        setZoomPoints(ZERO_F, ZERO_F, oldSeedR, oldSeedI);
        scalar_full_t offsetReal = ZERO_F;
        scalar_full_t offsetImag = ZERO_F;
        const Backend::Status offsetStatus = _pointFromPixel(
            pixelX, pixelY, offsetReal, offsetImag);

        setZoomGlobals(oldCount, oldZoom);
        setZoomPoints(oldPointR, oldPointI, oldSeedR, oldSeedI);

        if (!offsetStatus) return offsetStatus;

        real = toScalarString(targetReal - offsetReal);
        imag = toScalarString(offsetImag - targetImag);

        return Backend::Status::success();
#endif
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

    Backend::Status setPoint(const std::string &real,
        const std::string &imag) override {
        if (!isValidFullInput(real) || !isValidFullInput(imag)) {
            return makeFailure("Point coordinates must be valid numbers.");
        }

        _pointReal = real;
        _pointImag = imag;

#if defined(USE_MPFR)
        MPFRGlobals::initMPFRValues(_pointReal.c_str(), _pointImag.c_str());
        setZoomPoints(
            static_cast<scalar_full_t>(mpfr_get_d(MPFRGlobals::point_r_mp, MPFRTypes::ROUNDING)),
            static_cast<scalar_full_t>(mpfr_get_d(MPFRGlobals::point_i_mp, MPFRTypes::ROUNDING)),
            seed_r,
            seed_i
        );
#elif defined(USE_QD)
        QDGlobals::initQDValues(
            _pointReal.c_str(), _pointImag.c_str());
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
        scalar_full_t real = ZERO_F;
        scalar_full_t imag = ZERO_F;

        if (auto status = _pointFromPixel(pixelX, pixelY, real, imag); !status) {
            return status;
        }

        _pointReal = toScalarString(real);
        _pointImag = toScalarString(imag);

        setZoomPoints(real, imag, seed_r, seed_i);
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
        mpfr_inits2(MPFRTypes::getPrecisionBits(),
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
        qd_real seedRealQd;
        qd_real seedImagQd;
        if (!QDTypes::parseNumber(seedRealQd, _seedReal.c_str()) ||
            !QDTypes::parseNumber(seedImagQd, _seedImag.c_str())) {
            return makeFailure("Seed coordinates must be valid numbers.");
        }

        setZoomPoints(
            point_r,
            point_i,
            static_cast<scalar_full_t>(to_double(seedRealQd)),
            static_cast<scalar_full_t>(to_double(seedImagQd))
        );
#else
        setZoomPoints(
            point_r,
            point_i,
            PARSE_F(_seedReal.c_str()),
            PARSE_F(_seedImag.c_str())
        );
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
        scalar_full_t real = ZERO_F;
        scalar_full_t imag = ZERO_F;

        if (auto status = _pointFromPixel(pixelX, pixelY, real, imag); !status) {
            return status;
        }

        _seedReal = toScalarString(real);
        _seedImag = toScalarString(imag);

        setZoomPoints(point_r, point_i, real, imag);
        return Backend::Status::success();
#endif
    }

    Backend::Status setFractalType(Backend::FractalType type) override {
        ScalarGlobals::setFractalType(
            static_cast<int>(type),
            isJuliaSet, isInverse
        );
        return Backend::Status::success();
    }

    void setFractalMode(bool juliaSet, bool inverse) override {
        ScalarGlobals::setFractalType(fractalType, juliaSet, inverse);
    }

    Backend::Status setFractalExponent(const std::string &exponent) override {
        if (!isValidFullInput(exponent)) {
            return makeFailure("Fractal exponent must be a valid number.");
        }

        _exponent = exponent;
        if (!ScalarGlobals::setFractalExponent(PARSE_F(_exponent.c_str()))) {
            return makeFailure("Fractal exponent must be > 1.");
        }

        return Backend::Status::success();
    }

    Backend::Status setColorMethod(Backend::ColorMethod method) override {
        ScalarGlobals::setColorMethod(static_cast<int>(method));
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
        scalar_full_t real = ZERO_F;
        scalar_full_t imag = ZERO_F;

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

            scalar_half_t r = ZERO_H;
            scalar_half_t g = ZERO_H;
            scalar_half_t b = ZERO_H;
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
        if (auto status = _ensureImage(); !status)
            return status;

        VectorGlobals::initVectors();
        MPFRGlobals::initMPFRValues(_pointReal.c_str(), _pointImag.c_str());
        QDGlobals::initQDValues(
            _pointReal.c_str(), _pointImag.c_str());

        renderImage(_image.get(), &_callbacks,
            static_cast<bool>(_callbacks.onProgress),
            static_cast<bool>(_callbacks.onInfo));

        return Backend::Status::success();
    }

    void clearImage() override {
        if (_image) _image->clear();
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
    std::unique_ptr<Image> _image;
    std::unique_ptr<Image> _palettePreviewImage;
    std::unique_ptr<Image> _sinePreviewImage;
    int _lastAAPixels = 1;

    std::string _pointReal = "0";
    std::string _pointImag = "0";
    std::string _seedReal = "0";
    std::string _seedImag = "0";
    std::string _exponent = "2";

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
            if (!isValidPaletteColor(parsed)) {
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

    Backend::Status _pointFromPixel(int pixelX, int pixelY,
        scalar_full_t &real, scalar_full_t &imag) const {
        if (outputWidth <= 0 || outputHeight <= 0) {
            return makeFailure("Image size must be set before using pixel coordinates.");
        }

        real = getOutputCenterReal(pixelX);
        imag = getOutputCenterImag(pixelY);
        return Backend::Status::success();
    }

    Backend::Status _pointFromPixelString(int pixelX, int pixelY,
        std::string &real, std::string &imag) const {
#if defined(USE_MPFR)
        if (outputWidth <= 0 || outputHeight <= 0) {
            return makeFailure("Image size must be set before using pixel coordinates.");
        }

        MPFRGlobals::initMPFRValues(_pointReal.c_str(), _pointImag.c_str());

        mpfr_t realMp, imagMp;
        mpfr_inits2(MPFRTypes::getPrecisionBits(),
            realMp, imagMp, static_cast<mpfr_ptr>(nullptr));

        getOutputCenterReal_mp(realMp, pixelX);
        getOutputCenterImag_mp(imagMp, pixelY);

        real = toMpfrString(realMp);
        imag = toMpfrString(imagMp);

        mpfr_clears(realMp, imagMp, static_cast<mpfr_ptr>(nullptr));
        return Backend::Status::success();
#elif defined(USE_QD)
        if (outputWidth <= 0 || outputHeight <= 0) {
            return makeFailure("Image size must be set before using pixel coordinates.");
        }

        QDGlobals::initQDValues(
            _pointReal.c_str(), _pointImag.c_str());

        qd_real realQd;
        qd_real imagQd;
        getOutputCenterReal_qd(realQd, pixelX);
        getOutputCenterImag_qd(imagQd, pixelY);

        real = toQdString(realQd);
        imag = toQdString(imagQd);
        return Backend::Status::success();
#else
        scalar_full_t realVal = ZERO_F;
        scalar_full_t imagVal = ZERO_F;
        if (auto status = _pointFromPixel(pixelX, pixelY, realVal, imagVal); !status) {
            return status;
        }

        real = toScalarString(realVal);
        imag = toScalarString(imagVal);
        return Backend::Status::success();
#endif
    }

    Backend::Status _ensureImage() {
        const bool needsNewImage = !_image ||
            _image->outputWidth() != outputWidth ||
            _image->outputHeight() != outputHeight ||
            _lastAAPixels != aaPixels;

        if (needsNewImage) {
            if (auto image = Image::create(outputWidth, outputHeight,
                useThreads, aaPixels); !image)
                return makeFailure("Failed to allocate image.");
            else _image = std::move(image);

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