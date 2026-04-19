#include "mandelbrotgui.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <cmath>
#include <optional>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QInputDialog>
#include <QIntValidator>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QScreen>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWindow>

#include "CPUInfo.h"
#include "parsers/palette/PaletteParser.h"
#include "parsers/palette/PaletteWriter.h"
#include "parsers/point/PointParser.h"
#include "parsers/point/PointWriter.h"
#include "parsers/sine/SineParser.h"
#include "parsers/sine/SineWriter.h"
#include "prefix.h"
#include "util/FormatUtil.h"

namespace {
    constexpr auto kDefaultPaletteName = "default";
    constexpr auto kDefaultSineName = "default";
    constexpr double kMinGuiZoom = -3.2499;
    constexpr int kControlScrollBarWidth = 10;
    constexpr int kControlWindowScreenPadding = 48;

    constexpr std::array<double, 21> kZoomStepTable = {
        1.000625, 1.00125, 1.0025, 1.005, 1.01, 1.015, 1.02, 1.025, 1.03, 1.04,
        1.05, 1.06, 1.07, 1.08, 1.09, 1.10, 1.11, 1.12, 1.14, 1.17, 1.20
    };

    struct PaletteStop {
        int id = 0;
        double pos = 0.0;
        QColor color;
    };

    struct SaveDialogResult {
        QString path;
        QString type;
        bool appendDate = false;
    };

    int clampSliderValue(int value) {
        return std::clamp(value, 0, 20);
    }

    QString appendExtensionIfMissing(const QString &path, const QString &type) {
        const QString lowered = path.toLower();
        if (lowered.endsWith('.' + type)) return path;
        if (type == "jpg" && lowered.endsWith(".jpeg")) return path;
        return path + '.' + type;
    }

    double clampGuiZoom(double zoom) {
        return std::max(zoom, kMinGuiZoom);
    }

    QString formatImageMemoryText(const Backend::ImageEvent &event) {
        const QString renderBytes = QString::fromStdString(
            FormatUtil::formatBufferSize(event.primaryBytes));
        if (!event.downscaling) {
            return QString("Render: %1").arg(renderBytes);
        }

        const QString outputBytesText = QString::fromStdString(
            FormatUtil::formatBufferSize(event.secondaryBytes));
        return QString("Render: %1  Output: %2")
            .arg(renderBytes, outputBytesText);
    }

    QColor lightColorToQColor(const Backend::LightColor &color) {
        return QColor::fromRgbF(
            std::clamp(static_cast<double>(color.R), 0.0, 1.0),
            std::clamp(static_cast<double>(color.G), 0.0, 1.0),
            std::clamp(static_cast<double>(color.B), 0.0, 1.0));
    }

    Backend::LightColor lightColorFromQColor(const QColor &color) {
        return {
            .R = static_cast<float>(color.redF()),
            .G = static_cast<float>(color.greenF()),
            .B = static_cast<float>(color.blueF())
        };
    }

    QString lightColorButtonStyle(const QColor &color) {
        const int luminance =
            (color.red() * 299 + color.green() * 587 + color.blue() * 114) / 1000;
        const QColor fg = luminance >= 160 ? QColor(Qt::black) : QColor(Qt::white);

        return QString(
            "QPushButton {"
            " background-color: %1;"
            " color: %2;"
            "}").arg(color.name(QColor::HexRgb), fg.name(QColor::HexRgb));
    }

    Backend::PaletteHexConfig defaultPalette() {
        Backend::PaletteHexConfig palette;
        palette.totalLength = 10.0f;
        palette.offset = 0.0f;
        palette.blendEnds = true;
        palette.entries = {
            { "#1C0F52", 1.0f },
            { "#2F67C8", 1.0f },
            { "#BDE9FF", 1.0f },
            { "#FFD86B", 1.0f },
            { "#A62F14", 1.0f }
        };
        return palette;
    }

    Backend::SinePaletteConfig defaultSinePalette() {
        return {};
    }

    std::filesystem::path sineDirectoryPath() {
        return executableDir() / "sines";
    }

    std::filesystem::path sineFilePath(const QString &name) {
        return sineDirectoryPath() /
            std::filesystem::path((name + ".txt").toStdString());
    }

    std::filesystem::path paletteDirectoryPath() {
        return executableDir() / "palettes";
    }

    std::filesystem::path paletteFilePath(const QString &name) {
        return paletteDirectoryPath() /
            std::filesystem::path((name + ".txt").toStdString());
    }

    std::filesystem::path pointDirectoryPath() {
        return executableDir() / "points";
    }

    bool ensurePaletteDirectory(QString &errorMessage) {
        errorMessage.clear();

        std::error_code ec;
        std::filesystem::create_directories(paletteDirectoryPath(), ec);
        if (!ec) return true;

        errorMessage = QString("Failed to create palette directory: %1")
            .arg(QString::fromStdString(ec.message()));
        return false;
    }

    QString normalizePaletteName(const QString &name) {
        return name.trimmed();
    }

    bool isValidPaletteName(const QString &name) {
        const QString trimmed = normalizePaletteName(name);
        if (trimmed.isEmpty()) return false;
        if (trimmed == "." || trimmed == "..") return false;

        for (const QChar ch : trimmed) {
            if (ch.unicode() > 0x7F) return false;
            if (ch.isLetterOrNumber()) continue;
            if (ch == ' ' || ch == '_' || ch == '-' || ch == '.') continue;
            return false;
        }

        return true;
    }

    QString sanitizePaletteName(const QString &name) {
        QString sanitized = normalizePaletteName(name);
        for (QChar &ch : sanitized) {
            const bool allowed = ch.unicode() <= 0x7F &&
                (ch.isLetterOrNumber() || ch == ' ' || ch == '_' ||
                 ch == '-' || ch == '.');
            if (!allowed) ch = '_';
        }

        sanitized = sanitized.simplified();
        if (sanitized.isEmpty() || sanitized == "." || sanitized == "..") {
            sanitized = "palette";
        }

        return sanitized;
    }

    QStringList listPaletteNames() {
        QStringList names;
        std::error_code ec;
        const std::filesystem::path dir = paletteDirectoryPath();
        if (!std::filesystem::exists(dir, ec) || ec) {
            return names;
        }

        for (const auto &entry : std::filesystem::directory_iterator(dir, ec)) {
            if (ec) break;
            if (!entry.is_regular_file()) continue;

            const std::filesystem::path path = entry.path();
            if (path.extension() != ".txt") continue;
            names.push_back(QString::fromStdWString(path.stem().wstring()));
        }

        names.removeDuplicates();
        std::sort(names.begin(), names.end(), [](const QString &a, const QString &b) {
            if (a.compare(kDefaultPaletteName, Qt::CaseInsensitive) == 0) return true;
            if (b.compare(kDefaultPaletteName, Qt::CaseInsensitive) == 0) return false;
            return a.compare(b, Qt::CaseInsensitive) < 0;
        });
        return names;
    }

    QString uniquePaletteName(const QString &baseName,
        const QStringList &existingNames) {
        const QString sanitized = sanitizePaletteName(baseName);
        if (!existingNames.contains(sanitized, Qt::CaseInsensitive)) {
            return sanitized;
        }

        for (int suffix = 2;; ++suffix) {
            const QString candidate = QString("%1 %2")
                .arg(sanitized)
                .arg(suffix);
            if (!existingNames.contains(candidate, Qt::CaseInsensitive)) {
                return candidate;
            }
        }
    }

    QStringList listSineNames() {
        QStringList names;
        std::error_code ec;
        const std::filesystem::path dir = sineDirectoryPath();
        if (!std::filesystem::exists(dir, ec) || ec) {
            return names;
        }

        for (const auto &entry : std::filesystem::directory_iterator(dir, ec)) {
            if (ec) break;
            if (!entry.is_regular_file()) continue;

            const std::filesystem::path path = entry.path();
            if (path.extension() != ".txt") continue;
            names.push_back(QString::fromStdWString(path.stem().wstring()));
        }

        names.removeDuplicates();
        std::sort(names.begin(), names.end(), [](const QString &a, const QString &b) {
            if (a.compare(kDefaultSineName, Qt::CaseInsensitive) == 0) return true;
            if (b.compare(kDefaultSineName, Qt::CaseInsensitive) == 0) return false;
            return a.compare(b, Qt::CaseInsensitive) < 0;
        });
        return names;
    }

    bool ensureSineDirectory(QString &errorMessage) {
        errorMessage.clear();

        std::error_code ec;
        std::filesystem::create_directories(sineDirectoryPath(), ec);
        if (!ec) return true;

        errorMessage = QString("Failed to create sine directory: %1")
            .arg(QString::fromStdString(ec.message()));
        return false;
    }

    bool loadSineFromPath(const std::filesystem::path &path,
        Backend::SinePaletteConfig &palette, QString &errorMessage) {
        SineParser parser("-");
        std::string err;
        if (parser.parse({ path.string() }, palette, err)) {
            errorMessage.clear();
            return true;
        }

        errorMessage = QString::fromStdString(err);
        return false;
    }

    bool saveSineToPath(const std::filesystem::path &path,
        const Backend::SinePaletteConfig &palette, QString &errorMessage) {
        SineWriter writer(palette);
        std::string err;
        if (writer.write(path.string(), err)) {
            errorMessage.clear();
            return true;
        }

        errorMessage = QString::fromStdString(err);
        return false;
    }

    bool loadPaletteFromPath(const std::filesystem::path &path,
        Backend::PaletteHexConfig &palette, QString &errorMessage) {
        PaletteParser parser("-");
        std::string err;
        if (parser.parse({ path.string() }, palette, err)) {
            errorMessage.clear();
            return true;
        }

        errorMessage = QString::fromStdString(err);
        return false;
    }

    bool savePaletteToPath(const std::filesystem::path &path,
        const Backend::PaletteHexConfig &palette, QString &errorMessage) {
        PaletteWriter writer(palette);
        std::string err;
        if (writer.write(path.string(), err)) {
            errorMessage.clear();
            return true;
        }

        errorMessage = QString::fromStdString(err);
        return false;
    }

    bool ensurePointDirectory(QString &errorMessage) {
        errorMessage.clear();

        std::error_code ec;
        std::filesystem::create_directories(pointDirectoryPath(), ec);
        if (!ec) return true;

        errorMessage = QString("Failed to create points directory: %1")
            .arg(QString::fromStdString(ec.message()));
        return false;
    }

    bool loadPointFromPath(const std::filesystem::path &path,
        PointConfig &point, QString &errorMessage) {
        PointParser parser;
        std::string err;
        if (parser.parse(path.string(), point, err)) {
            errorMessage.clear();
            return true;
        }

        errorMessage = QString::fromStdString(err);
        return false;
    }

    bool savePointToPath(const std::filesystem::path &path,
        const PointConfig &point, QString &errorMessage) {
        PointWriter writer(point);
        std::string err;
        if (writer.write(path.string(), err)) {
            errorMessage.clear();
            return true;
        }

        errorMessage = QString::fromStdString(err);
        return false;
    }

    void stabilizePushButton(QPushButton *button) {
        if (!button) return;
        button->setAutoDefault(false);
        button->setDefault(false);
    }

    void setLayoutItemsVisible(QLayout *layout, bool visible) {
        if (!layout) return;

        for (int i = 0; i < layout->count(); ++i) {
            QLayoutItem *item = layout->itemAt(i);
            if (!item) continue;

            if (QWidget *widget = item->widget()) {
                widget->setVisible(visible);
            } else if (QLayout *childLayout = item->layout()) {
                setLayoutItemsVisible(childLayout, visible);
            }
        }
    }

    void setLayoutItemsEnabled(QLayout *layout, bool enabled) {
        if (!layout) return;

        for (int i = 0; i < layout->count(); ++i) {
            QLayoutItem *item = layout->itemAt(i);
            if (!item) continue;

            if (QWidget *widget = item->widget()) {
                widget->setEnabled(enabled);
            } else if (QLayout *childLayout = item->layout()) {
                setLayoutItemsEnabled(childLayout, enabled);
            }
        }
    }

    class CollapsibleGroupBox final : public QGroupBox {
    public:
        explicit CollapsibleGroupBox(const QString &title,
            bool expanded = true,
            std::function<void()> onToggled = {},
            QWidget *parent = nullptr)
            : QGroupBox(title, parent),
              _onToggled(std::move(onToggled)) {
            setCheckable(true);
            setChecked(expanded);
            setFlat(true);

            QObject::connect(this, &QGroupBox::toggled, this, [this](bool checked) {
                applyExpandedState(checked);
                if (_onToggled) _onToggled();
            });
        }

        void applyExpandedState(bool expanded) {
            if (layout()) {
                setLayoutItemsVisible(layout(), expanded);
            }
        }

        void setContentEnabled(bool enabled) {
            if (layout()) {
                setLayoutItemsEnabled(layout(), enabled);
            }

            setEnabled(true);

            QPalette groupPalette = palette();
            const QPalette appPalette = QApplication::palette(this);
            groupPalette.setColor(
                QPalette::WindowText,
                appPalette.color(enabled ? QPalette::WindowText : QPalette::Mid));
            setPalette(groupPalette);
        }

    private:
        std::function<void()> _onToggled;
    };

    QImage imageViewToImage(const Backend::ImageView &view) {
        if (!view.outputPixels || view.outputWidth <= 0 || view.outputHeight <= 0) {
            return {};
        }

        const int bytesPerLine = view.downscaling ?
            view.outputWidth * 3 : view.strideWidth;
        QImage wrapped(view.outputPixels,
            view.outputWidth,
            view.outputHeight,
            bytesPerLine,
            QImage::Format_RGB888);
        return wrapped.copy();
    }

    double effectiveDevicePixelRatio(const QWidget *widget) {
        if (!widget) return 1.0;

        if (const QWindow *window = widget->windowHandle()) {
            return std::max(1.0, window->devicePixelRatio());
        }
        if (const QScreen *screen = widget->screen()) {
            return std::max(1.0, screen->devicePixelRatio());
        }
        return std::max(1.0, widget->devicePixelRatioF());
    }

    void resizeViewportToImageSize(QWidget *viewport, const QSize &imageSize) {
        if (!viewport || !imageSize.isValid()) return;

        const double dpr = effectiveDevicePixelRatio(viewport);
        const QSize logicalSize(
            std::max(1, static_cast<int>(std::lround(
                static_cast<double>(imageSize.width()) / dpr))),
            std::max(1, static_cast<int>(std::lround(
                static_cast<double>(imageSize.height()) / dpr))));

        viewport->showNormal();
        viewport->setMinimumSize(logicalSize);
        viewport->setMaximumSize(logicalSize);
        viewport->resize(logicalSize);
    }

    double paletteSegmentLengthSum(const Backend::PaletteHexConfig &palette) {
        if (palette.entries.empty()) return 0.0;

        const size_t segmentCount = palette.blendEnds ?
            palette.entries.size() :
            palette.entries.size() - 1;

        double total = 0.0;
        for (size_t i = 0; i < segmentCount; ++i) {
            total += std::max(0.0f, palette.entries[i].length);
        }

        return total;
    }

    QColor lerpPreviewColor(const QColor &from, const QColor &to, double t) {
        const double clamped = std::clamp(t, 0.0, 1.0);
        return QColor(
            static_cast<int>(std::lround(from.red() +
                (to.red() - from.red()) * clamped)),
            static_cast<int>(std::lround(from.green() +
                (to.green() - from.green()) * clamped)),
            static_cast<int>(std::lround(from.blue() +
                (to.blue() - from.blue()) * clamped)));
    }

    QColor samplePalettePreviewColor(
        const Backend::PaletteHexConfig &palette,
        double x
    ) {
        if (palette.entries.empty()) return Qt::black;

        const size_t colorCount = palette.entries.size();
        size_t segmentCount = colorCount;
        if (!palette.blendEnds) {
            if (colorCount < 2) {
                return QColor(QString::fromStdString(palette.entries.front().color));
            }
            segmentCount--;
        }

        if (segmentCount == 0) {
            return QColor(QString::fromStdString(palette.entries.front().color));
        }

        std::vector<QColor> colors;
        colors.reserve(colorCount);
        for (const auto &entry : palette.entries) {
            QColor color(QString::fromStdString(entry.color));
            colors.push_back(color.isValid() ? color : QColor(Qt::black));
        }

        std::vector<double> accum(segmentCount + 1, 0.0);
        std::vector<double> spans(segmentCount, 0.0);
        const double totalLength =
            std::max(1.0, static_cast<double>(palette.totalLength));
        const double lengthSum = paletteSegmentLengthSum(palette);
        const double defaultSpan = totalLength / static_cast<double>(segmentCount);

        for (size_t i = 0; i < segmentCount; ++i) {
            const double rawLength = std::max(0.0,
                static_cast<double>(palette.entries[i].length));
            spans[i] = lengthSum > 0.0
                ? rawLength / lengthSum * totalLength
                : defaultSpan;
            accum[i + 1] = accum[i] + spans[i];
        }

        const double wrappedTotal = palette.blendEnds
            ? totalLength
            : std::max(accum.back(), 1.0e-12);
        const double offset = std::clamp(
            static_cast<double>(palette.offset),
            0.0,
            wrappedTotal);

        double t = std::fmod(x + offset, wrappedTotal);
        if (t < 0.0) t += wrappedTotal;
        if (t >= wrappedTotal) {
            t = std::nextafter(wrappedTotal, 0.0);
        }

        size_t idx = 0;
        while (idx + 1 < segmentCount && accum[idx + 1] <= t) {
            idx++;
        }

        const double span = spans[idx];
        const double u = span > 0.0 ? (t - accum[idx]) / span : 0.0;

        size_t next = idx + 1;
        if (palette.blendEnds) {
            next %= colors.size();
        } else if (next >= colors.size()) {
            next = colors.size() - 1;
        }

        return lerpPreviewColor(colors[idx], colors[next], u);
    }

    QImage makePalettePreviewImage(
        const Backend::PaletteHexConfig &palette,
        int width,
        int height
    ) {
        if (width <= 0 || height <= 0) return {};

        QImage image(width, height, QImage::Format_RGB32);
        const double totalLength =
            std::max(1.0, static_cast<double>(palette.totalLength));
        const double denom = width > 1 ? static_cast<double>(width - 1) : 1.0;

        for (int x = 0; x < width; ++x) {
            const QColor color = samplePalettePreviewColor(
                palette,
                totalLength * static_cast<double>(x) / denom);
            const QRgb pixel = color.rgb();

            for (int y = 0; y < height; ++y) {
                image.setPixel(x, y, pixel);
            }
        }

        return image;
    }

    std::vector<PaletteStop> configToStops(const Backend::PaletteHexConfig &palette) {
        std::vector<PaletteStop> stops;
        if (palette.entries.empty()) return stops;

        double accum = 0.0;
        const double segmentSum = paletteSegmentLengthSum(palette);
        const bool useEqualSpacing = segmentSum <= 0.0;
        const size_t stopCount = palette.entries.size();
        int nextId = 1;

        if (!palette.blendEnds && palette.entries.size() >= 2) {
            for (size_t i = 0; i + 1 < palette.entries.size(); ++i) {
                const auto &entry = palette.entries[i];
                stops.push_back({
                    nextId++,
                    useEqualSpacing ?
                        static_cast<double>(i) /
                            std::max<size_t>(1, palette.entries.size() - 1) :
                        std::clamp(accum / segmentSum, 0.0, 1.0),
                    QColor(QString::fromStdString(entry.color))
                });
                accum += std::max(0.0f, entry.length);
            }

            stops.push_back({
                nextId++,
                1.0,
                QColor(QString::fromStdString(palette.entries.back().color))
            });
            return stops;
        }

        for (const auto &entry : palette.entries) {
            stops.push_back({
                nextId++,
                useEqualSpacing ?
                    static_cast<double>(stops.size()) / std::max<size_t>(1, stopCount) :
                    std::clamp(accum / segmentSum, 0.0, 1.0),
                QColor(QString::fromStdString(entry.color))
            });
            accum += std::max(0.0f, entry.length);
        }

        if (stops.size() == 1) {
            stops.push_back({ nextId++, 1.0, stops.front().color });
        }

        return stops;
    }

    Backend::PaletteHexConfig stopsToConfig(const std::vector<PaletteStop> &stops,
        float totalLength, float offset, bool blendEnds) {
        constexpr double kLoopEndpointEpsilon = 1e-4;
        Backend::PaletteHexConfig palette;
        palette.totalLength = totalLength;
        palette.offset = offset;
        palette.blendEnds = blendEnds;

        if (stops.size() < 2) {
            const auto fallback = defaultPalette();
            palette.entries = fallback.entries;
            return palette;
        }

        std::vector<PaletteStop> sorted = stops;
        std::sort(sorted.begin(), sorted.end(),
            [](const PaletteStop &a, const PaletteStop &b) {
                return a.pos < b.pos;
            });

        if (blendEnds && sorted.size() >= 2 && sorted.back().pos >= 1.0) {
            const double minPos = std::min(
                1.0 - kLoopEndpointEpsilon,
                sorted[sorted.size() - 2].pos + kLoopEndpointEpsilon);
            sorted.back().pos = minPos;
        }

        if (!blendEnds) {
            for (size_t i = 0; i + 1 < sorted.size(); ++i) {
                const double start = std::clamp(sorted[i].pos, 0.0, 1.0);
                const double end = std::clamp(sorted[i + 1].pos, 0.0, 1.0);
                const double len = std::max((end - start) * totalLength, 0.0001);

                palette.entries.push_back({
                    sorted[i].color.name(QColor::HexRgb).toStdString(),
                    static_cast<float>(len)
                });
            }

            palette.entries.push_back({
                sorted.back().color.name(QColor::HexRgb).toStdString(),
                0.0f
            });
            return palette;
        }

        for (size_t i = 0; i < sorted.size(); ++i) {
            const size_t next = (i + 1) % sorted.size();
            double len = sorted[next].pos - sorted[i].pos;
            if (next == 0) len += 1.0;
            len = std::max(len * totalLength, 0.0001);

            palette.entries.push_back({
                sorted[i].color.name(QColor::HexRgb).toStdString(),
                static_cast<float>(len)
            });
        }

        return palette;
    }

    std::optional<SaveDialogResult> runSaveDialog(
        QWidget *parent,
        const QString &suggestedFile
    ) {
        QFileDialog dialog(parent, "Save Image");
        dialog.setAcceptMode(QFileDialog::AcceptSave);
        dialog.setOption(QFileDialog::DontUseNativeDialog, true);
        dialog.setNameFilters({
            "PNG Files (*.png)",
            "JPEG Files (*.jpg *.jpeg)",
            "Bitmap Files (*.bmp)"
        });
        dialog.selectNameFilter("PNG Files (*.png)");
        dialog.selectFile(suggestedFile);

        auto *appendDateCheck = new QCheckBox("Append Date", &dialog);
        if (auto *layout = dialog.layout()) {
            layout->addWidget(appendDateCheck);
        }

        if (dialog.exec() != QDialog::Accepted) return std::nullopt;

        SaveDialogResult result;
        result.path = dialog.selectedFiles().value(0);
        result.appendDate = appendDateCheck->isChecked();

        const QString filter = dialog.selectedNameFilter().toLower();
        if (filter.contains("*.jpg") || filter.contains("*.jpeg")) {
            result.type = "jpg";
        } else if (filter.contains("*.bmp")) {
            result.type = "bmp";
        } else {
            result.type = "png";
        }

        result.path = appendExtensionIfMissing(result.path, result.type);
        return result;
    }

    class SinePreviewWidget final : public QWidget {
    public:
        explicit SinePreviewWidget(QWidget *parent = nullptr)
            : QWidget(parent) {
            setFixedHeight(48);
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            setMouseTracking(true);
            setToolTip("Drag the middle to pan the range. Drag the edges to resize it.");
        }

        std::function<void(double, double)> onRangeChanged;

        [[nodiscard]] std::pair<double, double> range() const {
            return { _rangeMin, _rangeMax };
        }

        void setPreviewImage(const QImage &image) {
            _previewImage = image;
            update();
        }

        void resetRange(double minValue, double maxValue) {
            _rangeMin = minValue;
            _rangeMax = std::max(minValue + kMinSpan, maxValue);
            if (onRangeChanged) onRangeChanged(_rangeMin, _rangeMax);
            update();
        }

    protected:
        void paintEvent(QPaintEvent *) override {
            QPainter painter(this);
            painter.fillRect(rect(), palette().window());

            const QRect strip = previewRect();
            if (strip.width() <= 0 || strip.height() <= 0) return;
            const bool enabled = isEnabled();

            if (_previewImage.isNull()) {
                painter.fillRect(strip, palette().base());
            } else {
                if (enabled) {
                    painter.drawImage(strip, _previewImage);
                } else {
                    const QImage disabledImage = _previewImage
                        .convertToFormat(QImage::Format_Grayscale8)
                        .convertToFormat(QImage::Format_RGB32);
                    painter.drawImage(strip, disabledImage);
                    painter.fillRect(strip, QColor(255, 255, 255, 90));
                }
            }
            painter.setPen(enabled ? palette().dark().color() : palette().mid().color());
            painter.drawRect(strip.adjusted(0, 0, -1, -1));

            painter.setPen(QPen(
                enabled ? QColor(255, 255, 255, 180) : palette().mid().color(),
                1.0));
            painter.drawLine(strip.left(), strip.top(), strip.left(), strip.bottom());
            painter.drawLine(strip.right(), strip.top(), strip.right(), strip.bottom());

            drawHandle(painter, strip.left(), strip);
            drawHandle(painter, strip.right(), strip);

            painter.setPen(enabled ? palette().text().color() : palette().mid().color());
            painter.drawText(
                rect().adjusted(2, strip.bottom() + 3, -2, -2),
                Qt::AlignLeft | Qt::AlignVCenter,
                QString("Range %1 - %2")
                    .arg(QString::number(_rangeMin, 'f', 2))
                    .arg(QString::number(_rangeMax, 'f', 2)));
        }

        void mousePressEvent(QMouseEvent *event) override {
            if (event->button() == Qt::RightButton) {
                const QRect strip = previewRect();
                if (strip.contains(event->position().toPoint())) {
                    resetRange(kDefaultMin, kDefaultMax);
                }
                return;
            }

            if (event->button() != Qt::LeftButton) return;

            const QRect strip = previewRect();
            if (!strip.contains(event->position().toPoint())) return;

            _dragMode = hitTest(event->position().toPoint(), strip);
            _dragStartX = event->position().x();
            _dragRangeMin = _rangeMin;
            _dragRangeMax = _rangeMax;
            updateCursor(_dragMode);
        }

        void mouseMoveEvent(QMouseEvent *event) override {
            const QRect strip = previewRect();
            if (_dragMode == DragMode::none) {
                updateCursor(hitTest(event->position().toPoint(), strip));
                return;
            }

            if (strip.width() <= 0) return;

            const double span = std::max(kMinSpan, _dragRangeMax - _dragRangeMin);
            const double deltaValue =
                (event->position().x() - _dragStartX) / strip.width() * span;

            switch (_dragMode) {
                case DragMode::pan:
                {
                    double minValue = _dragRangeMin + deltaValue;
                    double maxValue = _dragRangeMax + deltaValue;
                    if (minValue < 0.0) {
                        maxValue -= minValue;
                        minValue = 0.0;
                    }
                    applyRange(minValue, maxValue);
                    break;
                }
                case DragMode::left:
                    applyRange(
                        std::clamp(_dragRangeMin + deltaValue, 0.0,
                            _dragRangeMax - kMinSpan),
                        _dragRangeMax);
                    break;
                case DragMode::right:
                    applyRange(
                        _dragRangeMin,
                        std::max(_dragRangeMin + kMinSpan, _dragRangeMax + deltaValue));
                    break;
                case DragMode::none:
                    break;
            }
        }

        void mouseReleaseEvent(QMouseEvent *) override {
            _dragMode = DragMode::none;
            updateCursor(DragMode::none);
        }

        void leaveEvent(QEvent *) override {
            if (_dragMode == DragMode::none) {
                unsetCursor();
            }
        }

    private:
        enum class DragMode {
            none,
            left,
            right,
            pan
        };

        static constexpr double kDefaultMin = 0.0;
        static constexpr double kDefaultMax = 100.0;
        static constexpr double kMinSpan = 1.0;
        static constexpr int kHandleHitWidth = 10;
        double _rangeMin = kDefaultMin;
        double _rangeMax = kDefaultMax;
        double _dragStartX = 0.0;
        double _dragRangeMin = kDefaultMin;
        double _dragRangeMax = kDefaultMax;
        DragMode _dragMode = DragMode::none;
        QImage _previewImage;

        QRect previewRect() const {
            return rect().adjusted(2, 2, -2, -16);
        }

        DragMode hitTest(const QPoint &point, const QRect &strip) const {
            if (!strip.contains(point)) return DragMode::none;
            if (std::abs(point.x() - strip.left()) <= kHandleHitWidth) return DragMode::left;
            if (std::abs(point.x() - strip.right()) <= kHandleHitWidth) return DragMode::right;
            return DragMode::pan;
        }

        void drawHandle(QPainter &painter, int x, const QRect &strip) const {
            const QRect handle(x - 2, strip.top() + 4, 4, std::max(6, strip.height() - 8));
            painter.fillRect(handle,
                isEnabled() ? QColor(255, 255, 255, 180) : palette().mid().color());
        }

        void updateCursor(DragMode mode) {
            switch (mode) {
                case DragMode::left:
                case DragMode::right:
                    setCursor(Qt::SizeHorCursor);
                    break;
                case DragMode::pan:
                    setCursor(Qt::OpenHandCursor);
                    break;
                case DragMode::none:
                    unsetCursor();
                    break;
            }
        }

        void applyRange(double minValue, double maxValue) {
            const double clampedMin = std::max(0.0, minValue);
            const double clampedMax = std::max(clampedMin + kMinSpan, maxValue);
            if (qFuzzyCompare(_rangeMin, clampedMin) &&
                qFuzzyCompare(_rangeMax, clampedMax)) {
                return;
            }

            _rangeMin = clampedMin;
            _rangeMax = clampedMax;
            if (onRangeChanged) onRangeChanged(_rangeMin, _rangeMax);
            update();
        }
    };

    class PalettePreviewWidget final : public QWidget {
    public:
        explicit PalettePreviewWidget(QWidget *parent = nullptr)
            : QWidget(parent) {
            setFixedHeight(30);
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        }

        void setPreviewPixmap(const QPixmap &pixmap) {
            _previewPixmap = pixmap;
            update();
        }

        void clearPreview() {
            _previewPixmap = {};
            update();
        }

        [[nodiscard]] QSize sizeHint() const override {
            return { 0, 30 };
        }

        [[nodiscard]] QSize minimumSizeHint() const override {
            return { 0, 30 };
        }

    protected:
        void paintEvent(QPaintEvent *) override {
            QPainter painter(this);
            const QRect frameRect = rect().adjusted(0, 0, -1, -1);
            painter.fillRect(rect(), palette().base());

            if (!_previewPixmap.isNull()) {
                const QPixmap scaled = _previewPixmap.scaled(
                    frameRect.size(),
                    Qt::IgnoreAspectRatio,
                    Qt::SmoothTransformation);
                if (isEnabled()) {
                    painter.drawPixmap(frameRect.topLeft(), scaled);
                } else {
                    const QImage disabledImage = scaled.toImage()
                        .convertToFormat(QImage::Format_Grayscale8)
                        .convertToFormat(QImage::Format_RGB32);
                    painter.drawImage(frameRect, disabledImage);
                    painter.fillRect(frameRect, QColor(255, 255, 255, 90));
                }
            }

            painter.setPen(isEnabled() ? palette().dark().color() : palette().mid().color());
            painter.drawRect(frameRect);
        }

    private:
        QPixmap _previewPixmap;
    };

    class PaletteTimelineWidget final : public QWidget {
    public:
        explicit PaletteTimelineWidget(QWidget *parent = nullptr)
            : QWidget(parent) {
            setMinimumHeight(0);
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            setMouseTracking(true);

            _positionEditor = new QLineEdit(this);
            _positionEditor->setFixedWidth(54);
            _positionEditor->setAlignment(Qt::AlignCenter);
            _positionEditor->setValidator(new QIntValidator(0, 100, _positionEditor));
            _positionEditor->hide();

            QObject::connect(_positionEditor, &QLineEdit::editingFinished,
                this, [this]() { commitPositionEdit(); });
        }

        std::function<void()> onChanged;
        std::function<void(int)> onSelectionChanged;
        std::function<void(int)> onEditColorRequested;

        void setBlendEnds(bool blendEnds) {
            _blendEnds = blendEnds;
            normalizeStopLayout();
            updatePositionEditor();
            update();
        }

        void setStops(const std::vector<PaletteStop> &stops) {
            _stops = stops;
            if (_stops.empty()) {
                _selectedIds.clear();
                _primarySelectedId.reset();
            } else if (!_primarySelectedId || !_selectedIds.contains(*_primarySelectedId)) {
                _setSingleSelection(_stops.front().id);
            }

            _sortStops();
            normalizeStopLayout();
            updatePositionEditor();
            update();
        }

        const std::vector<PaletteStop> &stops() const {
            return _stops;
        }

        int selectedIndex() const {
            if (!_primarySelectedId) return -1;

            for (int i = 0; i < static_cast<int>(_stops.size()); ++i) {
                if (_stops[i].id == *_primarySelectedId) return i;
            }

            return -1;
        }

        int selectedCount() const {
            return static_cast<int>(_selectedIds.size());
        }

        void setSelectedColor(const QColor &color) {
            if (_selectedIds.empty()) return;

            for (auto &stop : _stops) {
                if (_selectedIds.contains(stop.id)) {
                    stop.color = color;
                }
            }
            emitChanged();
            update();
        }

        void addStop() {
            _sortStops();
            normalizeStopLayout();

            PaletteStop stop;
            stop.id = ++_nextId;

            if (_stops.size() < 2) {
                stop.pos = 0.5;
            } else {
                double bestStart = 0.0;
                double bestEnd = _blendEnds ? _stops.front().pos + 1.0 : 1.0;
                double bestGap = -1.0;

                for (size_t i = 1; i < _stops.size(); ++i) {
                    const double start = _stops[i - 1].pos;
                    const double end = _stops[i].pos;
                    const double gap = end - start;
                    if (gap > bestGap) {
                        bestGap = gap;
                        bestStart = start;
                        bestEnd = end;
                    }
                }

                if (_blendEnds && _stops.size() >= 2) {
                    const double wrapStart = _stops.back().pos;
                    const double wrapEnd = _stops.front().pos + 1.0;
                    const double wrapGap = wrapEnd - wrapStart;
                    if (wrapGap > bestGap) {
                        bestGap = wrapGap;
                        bestStart = wrapStart;
                        bestEnd = wrapEnd;
                    }
                }

                stop.pos = (bestStart + bestEnd) * 0.5;
                if (stop.pos >= 1.0) stop.pos -= 1.0;
            }

            stop.color = sample(stop.pos);
            _stops.push_back(stop);
            _setSingleSelection(stop.id);
            _sortStops();
            normalizeStopLayout();
            emitSelectionChanged();
            emitChanged();
            update();
        }

        void removeSelectedStops() {
            if (_selectedIds.empty()) return;
            if (static_cast<int>(_stops.size()) - selectedCount() < 2) return;

            _stops.erase(std::remove_if(_stops.begin(), _stops.end(),
                [this](const PaletteStop &stop) {
                    return _selectedIds.contains(stop.id);
                }),
                _stops.end());

            _selectedIds.clear();
            if (_stops.empty()) {
                _primarySelectedId.reset();
            } else {
                _primarySelectedId = _stops.front().id;
                _selectedIds.insert(*_primarySelectedId);
            }

            normalizeStopLayout();
            emitSelectionChanged();
            emitChanged();
            update();
        }

        void evenSpacing() {
            if (_stops.size() < 2) return;

            _sortStops();
            for (size_t i = 0; i < _stops.size(); ++i) {
                if (_blendEnds) {
                    _stops[i].pos = static_cast<double>(i) /
                        static_cast<double>(_stops.size());
                } else {
                    const double denom = static_cast<double>(_stops.size() - 1);
                    _stops[i].pos = denom > 0.0 ? static_cast<double>(i) / denom : 0.0;
                }
            }

            normalizeStopLayout();
            emitChanged();
            update();
        }

    protected:
        void resizeEvent(QResizeEvent *event) override {
            QWidget::resizeEvent(event);
            updatePositionEditor();
        }

        void paintEvent(QPaintEvent *) override {
            QPainter painter(this);
            painter.fillRect(rect(), palette().window());

            const QRect grad = gradientRect();
            QLinearGradient gradient(grad.topLeft(), grad.topRight());
            for (const auto &stop : _stops) {
                gradient.setColorAt(std::clamp(stop.pos, 0.0, 1.0), stop.color);
            }
            if (!_stops.empty()) {
                gradient.setColorAt(1.0, _blendEnds ?
                    _stops.front().color :
                    _stops.back().color);
            }

            painter.fillRect(grad, gradient);
            painter.setPen(palette().dark().color());
            painter.drawRect(grad.adjusted(0, 0, -1, -1));

            for (const auto &stop : _stops) {
                const QPoint handlePos = stopPoint(stop.pos);
                QPainterPath path;
                path.moveTo(handlePos.x(), grad.bottom() + 4);
                path.lineTo(handlePos.x() - 6, grad.bottom() + 16);
                path.lineTo(handlePos.x() + 6, grad.bottom() + 16);
                path.closeSubpath();

                painter.fillPath(path, stop.color);
                painter.setPen(_selectedIds.contains(stop.id) ?
                    QPen(Qt::black, 1.5) :
                    QPen(palette().mid().color(), 1.0));
                painter.drawPath(path);
            }
        }

        void mousePressEvent(QMouseEvent *event) override {
            if (event->button() == Qt::LeftButton) {
                const int hit = hitTest(event->position().toPoint());
                const bool toggleSelection =
                    event->modifiers().testFlag(Qt::ControlModifier);

                if (hit >= 0) {
                    const int id = _stops[hit].id;
                    if (toggleSelection) {
                        if (_selectedIds.contains(id) && _selectedIds.size() > 1) {
                            _selectedIds.erase(id);
                            if (_primarySelectedId == id) {
                                _primarySelectedId = *_selectedIds.begin();
                            }
                        } else {
                            _selectedIds.insert(id);
                            _primarySelectedId = id;
                        }
                    } else if (!_selectedIds.contains(id) || _selectedIds.size() != 1) {
                        _setSingleSelection(id);
                    }

                    _draggingId.reset();
                    _dragStartPositions.clear();
                    if (!isLockedId(id)) {
                        _draggingId = id;
                        _dragAnchorPos = posFromPixel(event->position().x());
                        for (const auto &stop : _stops) {
                            if (_selectedIds.contains(stop.id) && !isLockedId(stop.id)) {
                                _dragStartPositions[stop.id] = stop.pos;
                            }
                        }
                    }
                    emitSelectionChanged();
                } else if (gradientRect().contains(event->position().toPoint())) {
                    PaletteStop stop;
                    stop.id = ++_nextId;
                    stop.pos = posFromPixel(event->position().x());
                    stop.color = sample(stop.pos);

                    _stops.push_back(stop);
                    _setSingleSelection(stop.id);
                    _draggingId = stop.id;
                    _dragAnchorPos = stop.pos;
                    _dragStartPositions = { { stop.id, stop.pos } };
                    _sortStops();
                    normalizeStopLayout();
                    emitSelectionChanged();
                    emitChanged();
                } else if (!toggleSelection) {
                    _selectedIds.clear();
                    _primarySelectedId.reset();
                    emitSelectionChanged();
                }

                update();
            }

            if (event->button() == Qt::RightButton) {
                const int hit = hitTest(event->position().toPoint());
                if (hit >= 0) {
                    if (!_selectedIds.contains(_stops[hit].id)) {
                        _setSingleSelection(_stops[hit].id);
                    }
                    removeSelectedStops();
                }
            }
        }

        void mouseMoveEvent(QMouseEvent *event) override {
            if (!_draggingId) return;
            if (_dragStartPositions.empty()) return;

            const double targetPos = posFromPixel(event->position().x());
            double delta = targetPos - _dragAnchorPos;
            double minDelta = -1.0;
            double maxDelta = 1.0;
            for (const auto &[id, pos] : _dragStartPositions) {
                (void)id;
                minDelta = std::max(minDelta, -pos);
                maxDelta = std::min(maxDelta, 1.0 - pos);
            }

            if (minDelta > maxDelta) {
                delta = minDelta;
            } else {
                delta = std::clamp(delta, minDelta, maxDelta);
            }

            for (auto &stop : _stops) {
                const auto it = _dragStartPositions.find(stop.id);
                if (it != _dragStartPositions.end()) {
                    stop.pos = it->second + delta;
                }
            }

            _sortStops();
            emitChanged();
            updatePositionEditor();
            update();
        }

        void mouseReleaseEvent(QMouseEvent *) override {
            _draggingId.reset();
            _dragStartPositions.clear();
        }

        void mouseDoubleClickEvent(QMouseEvent *event) override {
            const int hit = hitTest(event->position().toPoint());
            if (hit < 0) return;

            _setSingleSelection(_stops[hit].id);
            emitSelectionChanged();
            if (onEditColorRequested) onEditColorRequested(hit);
            update();
        }

    private:
        QRect gradientRect() const {
            return rect().adjusted(12, 12, -12, -28);
        }

        QPoint stopPoint(double pos) const {
            const QRect grad = gradientRect();
            const int x = grad.left() +
                static_cast<int>(std::round(pos * grad.width()));

            return {
                std::clamp(x, grad.left(), grad.right()),
                grad.bottom()
            };
        }

        double posFromPixel(double x) const {
            const QRect grad = gradientRect();
            if (grad.width() <= 0) return 0.0;

            return std::clamp(
                (x - grad.left()) / grad.width(),
                0.0,
                1.0
            );
        }

        QColor sample(double pos) const {
            if (_stops.empty()) return Qt::white;

            std::vector<PaletteStop> sorted = _stops;
            std::sort(sorted.begin(), sorted.end(),
                [](const PaletteStop &a, const PaletteStop &b) {
                    return a.pos < b.pos;
                });

            if (_blendEnds && sorted.size() >= 2) {
                if (pos < sorted.front().pos || pos > sorted.back().pos) {
                    const double start = sorted.back().pos;
                    const double end = sorted.front().pos + 1.0;
                    const double samplePos = pos < sorted.front().pos ? pos + 1.0 : pos;
                    const double span = std::max(end - start, 1e-6);
                    const double t = (samplePos - start) / span;

                    return QColor::fromRgbF(
                        sorted.back().color.redF() +
                            (sorted.front().color.redF() -
                                sorted.back().color.redF()) * t,
                        sorted.back().color.greenF() +
                            (sorted.front().color.greenF() -
                                sorted.back().color.greenF()) * t,
                        sorted.back().color.blueF() +
                            (sorted.front().color.blueF() -
                                sorted.back().color.blueF()) * t
                    );
                }
            } else {
                if (pos <= sorted.front().pos) return sorted.front().color;
                if (pos >= sorted.back().pos) return sorted.back().color;
            }

            for (size_t i = 0; i + 1 < sorted.size(); ++i) {
                if (pos < sorted[i].pos || pos > sorted[i + 1].pos) continue;

                const double span = std::max(
                    sorted[i + 1].pos - sorted[i].pos,
                    1e-6
                );
                const double t = (pos - sorted[i].pos) / span;

                return QColor::fromRgbF(
                    sorted[i].color.redF() +
                        (sorted[i + 1].color.redF() - sorted[i].color.redF()) * t,
                    sorted[i].color.greenF() +
                        (sorted[i + 1].color.greenF() - sorted[i].color.greenF()) * t,
                    sorted[i].color.blueF() +
                        (sorted[i + 1].color.blueF() - sorted[i].color.blueF()) * t
                );
            }

            return sorted.back().color;
        }

        int hitTest(const QPoint &point) const {
            for (int i = 0; i < static_cast<int>(_stops.size()); ++i) {
                const QRect area(stopPoint(_stops[i].pos) - QPoint(8, 6),
                    QSize(16, 18));
                if (area.contains(point)) return i;
            }

            return -1;
        }

        void _sortStops() {
            std::sort(_stops.begin(), _stops.end(),
                [](const PaletteStop &a, const PaletteStop &b) {
                    return a.pos < b.pos;
                });
        }

        bool isLockedIndex(int index) const {
            if (index < 0 || index >= static_cast<int>(_stops.size())) return false;
            if (index == 0) return true;
            if (!_blendEnds && index == static_cast<int>(_stops.size()) - 1) return true;
            return false;
        }

        bool isLockedId(int id) const {
            for (int i = 0; i < static_cast<int>(_stops.size()); ++i) {
                if (_stops[i].id == id) return isLockedIndex(i);
            }
            return false;
        }

        double minimumStopSpacing() const {
            return 0.0;
        }

        void normalizeStopLayout() {
            if (_stops.empty()) return;

            _sortStops();
            _stops.front().pos = 0.0;

            const double minGap = minimumStopSpacing();
            const int firstMovable = 1;
            const int lastMovable = _blendEnds ?
                static_cast<int>(_stops.size()) - 1 :
                static_cast<int>(_stops.size()) - 2;

            if (!_blendEnds && _stops.size() >= 2) {
                _stops.back().pos = 1.0;
            }

            if (lastMovable >= firstMovable) {
                for (int i = firstMovable; i <= lastMovable; ++i) {
                    _stops[i].pos = std::max(
                        _stops[i].pos,
                        _stops[i - 1].pos + minGap);
                }

                const double upperBound = _blendEnds ? 1.0 - minGap : 1.0 - minGap;
                if (_stops[lastMovable].pos > upperBound) {
                    _stops[lastMovable].pos = upperBound;
                    for (int i = lastMovable - 1; i >= firstMovable; --i) {
                        _stops[i].pos = std::min(
                            _stops[i].pos,
                            _stops[i + 1].pos - minGap);
                    }
                }

                if (_stops[firstMovable].pos < minGap) {
                    const int movableCount = lastMovable - firstMovable + 1;
                    if (movableCount > 0) {
                        const double start = minGap;
                        const double end = _blendEnds ? 1.0 - minGap : 1.0 - minGap;
                        const double denom = movableCount + (_blendEnds ? 1.0 : 0.0);
                        for (int i = 0; i < movableCount; ++i) {
                            _stops[firstMovable + i].pos =
                                start + (end - start) *
                                static_cast<double>(i + 1) / std::max(1.0, denom);
                        }
                    }
                }
            }

            if (!_blendEnds && _stops.size() >= 2) {
                for (int i = 1; i + 1 < static_cast<int>(_stops.size()); ++i) {
                    const double minPos = _stops[i - 1].pos + minGap;
                    const double maxPos = _stops[i + 1].pos - minGap;
                    if (minPos > maxPos) {
                        _stops[i].pos = minPos;
                    } else {
                        _stops[i].pos = std::clamp(_stops[i].pos, minPos, maxPos);
                    }
                }
                _stops.back().pos = 1.0;
            }
        }

        void updatePositionEditor() {
            if (!_positionEditor) return;
            if (_selectedIds.size() != 1) {
                _positionEditor->hide();
                return;
            }

            const int index = selectedIndex();
            if (index < 0 || index >= static_cast<int>(_stops.size())) {
                _positionEditor->hide();
                return;
            }

            const QString text = QString::number(
                static_cast<int>(std::lround(_stops[index].pos * 100.0)));
            if (!_positionEditor->hasFocus()) {
                _positionEditor->setText(text);
            }

            const QRect grad = gradientRect();
            const QPoint handle = stopPoint(_stops[index].pos);
            const int x = std::clamp(handle.x() - _positionEditor->width() / 2,
                grad.left(),
                std::max(grad.left(), grad.right() - _positionEditor->width() + 1));
            const int y = std::max(0, grad.top() - _positionEditor->height() - 6);
            _positionEditor->move(x, y);
            _positionEditor->setReadOnly(isLockedIndex(index));
            _positionEditor->show();
            _positionEditor->raise();
        }

        void commitPositionEdit() {
            if (!_positionEditor || !_positionEditor->isVisible()) return;

            const int index = selectedIndex();
            if (index < 0 || index >= static_cast<int>(_stops.size())) return;
            if (isLockedIndex(index)) {
                updatePositionEditor();
                return;
            }

            bool ok = false;
            const int entered = _positionEditor->text().toInt(&ok);
            if (!ok) {
                updatePositionEditor();
                return;
            }

            double pos = std::clamp(static_cast<double>(entered) / 100.0, 0.0, 1.0);
            _stops[index].pos = pos;
            _sortStops();
            normalizeStopLayout();
            emitChanged();
            emitSelectionChanged();
            update();
        }

        void _setSingleSelection(int id) {
            _selectedIds.clear();
            _selectedIds.insert(id);
            _primarySelectedId = id;
        }

        void emitChanged() {
            if (onChanged) onChanged();
        }

        void emitSelectionChanged() {
            updatePositionEditor();
            if (onSelectionChanged) onSelectionChanged(selectedIndex());
        }

        bool _blendEnds = true;
        std::vector<PaletteStop> _stops;
        int _nextId = 100;
        std::optional<int> _primarySelectedId;
        std::unordered_set<int> _selectedIds;
        std::optional<int> _draggingId;
        double _dragAnchorPos = 0.0;
        std::unordered_map<int, double> _dragStartPositions;
        QLineEdit *_positionEditor = nullptr;
    };

    class PaletteEditorDialog final : public QDialog {
    public:
        PaletteEditorDialog(const Backend::PaletteHexConfig &palette,
            const QString &paletteName,
            QWidget *parent = nullptr)
            : QDialog(parent),
              _palette(palette),
              _savedPaletteName(normalizePaletteName(paletteName)) {
            setWindowTitle("Palette");
            setModal(true);
            resize(420, 200);

            auto *root = new QVBoxLayout(this);

            auto *nameLayout = new QFormLayout();
            _nameEdit = new QLineEdit(_savedPaletteName.isEmpty() ?
                kDefaultPaletteName : _savedPaletteName);
            _nameEdit->setMaxLength(64);
            _nameEdit->setValidator(new QRegularExpressionValidator(
                QRegularExpression("[A-Za-z0-9 _.\\-]*"),
                _nameEdit));
            _nameEdit->setPlaceholderText("ASCII palette name");
            nameLayout->addRow("Name", _nameEdit);
            root->addLayout(nameLayout);

            _timeline = new PaletteTimelineWidget();
            root->addWidget(_timeline, 1);

            auto *optionsRow = new QHBoxLayout();
            _blendEndsCheck = new QCheckBox("Blend Ends");
            _blendEndsCheck->setChecked(_palette.blendEnds);
            optionsRow->addWidget(_blendEndsCheck);
            optionsRow->addStretch(1);
            root->addLayout(optionsRow);

            auto *buttonRow = new QHBoxLayout();
            _addButton = new QPushButton("+");
            _removeButton = new QPushButton("-");
            _evenButton = new QPushButton("Even");
            _colorButton = new QPushButton("Color");
            _importButton = new QPushButton("Import");
            _saveButton = new QPushButton("Save");
            stabilizePushButton(_addButton);
            stabilizePushButton(_removeButton);
            stabilizePushButton(_evenButton);
            stabilizePushButton(_colorButton);
            stabilizePushButton(_importButton);
            stabilizePushButton(_saveButton);
            _addButton->setFixedWidth(34);
            _removeButton->setFixedWidth(34);
            buttonRow->addWidget(_addButton);
            buttonRow->addWidget(_removeButton);
            buttonRow->addWidget(_evenButton);
            buttonRow->addWidget(_colorButton);
            buttonRow->addStretch(1);
            buttonRow->addWidget(_importButton);
            buttonRow->addWidget(_saveButton);
            root->addLayout(buttonRow);

            auto *dialogButtons = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
            root->addWidget(dialogButtons);

            applyPalette(_palette);
            _timeline->onChanged = [this]() {
                _palette = currentPalette();
            };
            _timeline->onSelectionChanged = [this](int index) {
                _colorButton->setEnabled(index >= 0);
                _removeButton->setEnabled(_timeline->selectedCount() > 0 &&
                    static_cast<int>(_timeline->stops().size()) -
                    _timeline->selectedCount() >= 2);
            };
            _timeline->onEditColorRequested = [this](int) {
                editColor();
            };

            QObject::connect(_addButton, &QPushButton::clicked, this, [this]() {
                _timeline->addStop();
            });
            QObject::connect(_removeButton, &QPushButton::clicked, this, [this]() {
                _timeline->removeSelectedStops();
            });
            QObject::connect(_evenButton, &QPushButton::clicked, this, [this]() {
                _timeline->evenSpacing();
            });
            QObject::connect(_colorButton, &QPushButton::clicked, this, [this]() {
                editColor();
            });
            QObject::connect(_blendEndsCheck, &QCheckBox::toggled, this, [this](bool checked) {
                _palette.blendEnds = checked;
                _timeline->setBlendEnds(checked);
                _palette = currentPalette();
            });
            QObject::connect(_importButton, &QPushButton::clicked, this, [this]() {
                importPalette();
            });
            QObject::connect(_saveButton, &QPushButton::clicked, this, [this]() {
                savePalette();
            });
            QObject::connect(dialogButtons, &QDialogButtonBox::accepted,
                this, &QDialog::accept);
            QObject::connect(dialogButtons, &QDialogButtonBox::rejected,
                this, &QDialog::reject);

            _colorButton->setEnabled(_timeline->selectedIndex() >= 0);
            _removeButton->setEnabled(_timeline->selectedCount() > 0 &&
                static_cast<int>(_timeline->stops().size()) -
                _timeline->selectedCount() >= 2);
        }

        Backend::PaletteHexConfig palette() const {
            return currentPalette();
        }

        QString savedPaletteName() const {
            return _savedPaletteName;
        }

    private:
        Backend::PaletteHexConfig currentPalette() const {
            if (!_timeline) return _palette;

            return stopsToConfig(
                _timeline->stops(),
                _palette.totalLength,
                _palette.offset,
                _blendEndsCheck && _blendEndsCheck->isChecked()
            );
        }

        void applyPalette(const Backend::PaletteHexConfig &palette) {
            _palette = palette;
            if (_blendEndsCheck) {
                const QSignalBlocker blocker(_blendEndsCheck);
                _blendEndsCheck->setChecked(_palette.blendEnds);
            }
            if (_timeline) {
                _timeline->setBlendEnds(_palette.blendEnds);
                _timeline->setStops(configToStops(_palette));
            }
        }

        QString validatedPaletteName() const {
            const QString name = normalizePaletteName(
                _nameEdit ? _nameEdit->text() : QString());
            return isValidPaletteName(name) ? name : QString();
        }

        void editColor() {
            const int index = _timeline->selectedIndex();
            if (index < 0) return;

            const QColor initial = _timeline->stops()[index].color;
            const QColor color = QColorDialog::getColor(initial, this, "Color");
            if (!color.isValid()) return;

            _timeline->setSelectedColor(color);
        }

        void importPalette() {
            const QString sourcePath = QFileDialog::getOpenFileName(
                this,
                "Import Palette",
                QString(),
                "Palette Files (*.txt);;All Files (*.*)"
            );
            if (sourcePath.isEmpty()) return;

            Backend::PaletteHexConfig loaded;
            QString errorMessage;
            if (!loadPaletteFromPath(sourcePath.toStdString(), loaded, errorMessage)) {
                QMessageBox::warning(this, "Palette", errorMessage);
                return;
            }

            loaded.totalLength = _palette.totalLength;
            loaded.offset = _palette.offset;

            if (!ensurePaletteDirectory(errorMessage)) {
                QMessageBox::warning(this, "Palette", errorMessage);
                return;
            }

            const QStringList existingNames = listPaletteNames();
            const QString importedName = uniquePaletteName(
                QFileInfo(sourcePath).completeBaseName(),
                existingNames);
            const std::filesystem::path destinationPath = paletteFilePath(importedName);
            if (!savePaletteToPath(destinationPath, loaded, errorMessage)) {
                QMessageBox::warning(this, "Palette", errorMessage);
                return;
            }

            _savedPaletteName = importedName;
            if (_nameEdit) _nameEdit->setText(importedName);
            applyPalette(loaded);
        }

        void savePalette() {
            const QString paletteName = validatedPaletteName();
            if (paletteName.isEmpty()) {
                QMessageBox::warning(this, "Palette",
                    "Palette names must be non-empty ASCII and use only letters, numbers, spaces, '.', '-', or '_'.");
                return;
            }

            QString errorMessage;
            if (!ensurePaletteDirectory(errorMessage)) {
                QMessageBox::warning(this, "Palette", errorMessage);
                return;
            }

            _palette = currentPalette();
            const std::filesystem::path destinationPath = paletteFilePath(paletteName);
            if (!savePaletteToPath(destinationPath, _palette, errorMessage)) {
                QMessageBox::warning(this, "Palette", errorMessage);
                return;
            }

            _savedPaletteName = paletteName;
        }

        Backend::PaletteHexConfig _palette;
        QString _savedPaletteName;
        PaletteTimelineWidget *_timeline = nullptr;
        QLineEdit *_nameEdit = nullptr;
        QCheckBox *_blendEndsCheck = nullptr;
        QPushButton *_addButton = nullptr;
        QPushButton *_removeButton = nullptr;
        QPushButton *_evenButton = nullptr;
        QPushButton *_colorButton = nullptr;
        QPushButton *_importButton = nullptr;
        QPushButton *_saveButton = nullptr;
    };

    class ViewportWindow final : public QWidget {
    public:
        explicit ViewportWindow(mandelbrotgui *owner)
            : QWidget(nullptr), _owner(owner) {
            setWindowTitle("Viewport");
            setMouseTracking(true);
            setFocusPolicy(Qt::StrongFocus);

            _rtZoomTimer.setInterval(_owner ? _owner->realtimeZoomIntervalMs() : 30);
            QObject::connect(&_rtZoomTimer, &QTimer::timeout, this, [this]() {
                if (!_owner) return;
                _owner->zoomAtPixel(mapToOutputPixel(_lastMousePos), _rtZoomZoomIn, true);
            });

            _panRedrawTimer.setSingleShot(true);
            _panRedrawTimer.setInterval(_owner ? _owner->panRedrawIntervalMs() : 100);
            QObject::connect(&_panRedrawTimer, &QTimer::timeout, this, [this]() {
                if (!_panning) return;

                if (_owner && _owner->renderInFlight()) {
                    _panRedrawTimer.start();
                    return;
                }

                commitPanOffset();
                if (_panning) {
                    _panRedrawTimer.start();
                }
            });

            _zoomOutRedrawTimer.setSingleShot(true);
            _zoomOutRedrawTimer.setInterval(_owner ? _owner->panRedrawIntervalMs() : 100);
            QObject::connect(&_zoomOutRedrawTimer, &QTimer::timeout, this, [this]() {
                if (!_zoomOutDragActive) return;

                commitZoomOutPreview(false);
                if (_zoomOutDragActive && _zoomOutPendingCommit) {
                    _zoomOutRedrawTimer.start();
                }
            });
        }

        void clearPreviewOffset() {
            const bool panNeedsClear =
                !_panCommittedOffset.isNull() || (!_panning && !_panOffset.isNull());
            const bool zoomNeedsClear = _zoomOutCommittedScale > 1.0001;
            if (!panNeedsClear && !zoomNeedsClear) {
                return;
            }

            _panCommittedOffset = {};
            if (!_panning) {
                _panOffset = {};
            }
            _zoomOutCommittedScale = 1.0;
            update();
        }

    protected:
        void paintEvent(QPaintEvent *) override {
            QPainter painter(this);
            painter.fillRect(rect(), Qt::black);

            const QImage &image = _owner->previewImage();
            if (!image.isNull()) {
                QImage scaledImage = image;
                scaledImage.setDevicePixelRatio(devicePixelRatioF());
                const QPoint totalOffset = _panCommittedOffset + _panOffset;
                const double previewScale = _zoomOutCommittedScale *
                    ((_zoomOutDragActive && _zoomOutPreviewScale > 1.0) ?
                        _zoomOutPreviewScale : 1.0);
                if (previewScale > 1.0001) {
                    const qreal previewImageScale = static_cast<qreal>(
                        1.0 / previewScale);
                    const QPointF center(width() / 2.0, height() / 2.0);
                    painter.save();
                    painter.translate(center);
                    painter.scale(previewImageScale, previewImageScale);
                    painter.translate(-center);
                    painter.drawImage(QPointF(totalOffset), scaledImage);
                    painter.restore();
                } else {
                    painter.drawImage(QPointF(totalOffset), scaledImage);
                }
            }

            painter.setRenderHint(QPainter::Antialiasing, true);

            if (!_selectionRect.isNull()) {
                painter.setPen(QPen(QColor(255, 255, 255, 180),
                    1.0, Qt::DashLine));
                painter.drawRect(_selectionRect.normalized());
            }

            const QString statusText = _owner->viewportStatusText();
            constexpr int overlayMargin = 8;
            constexpr int overlayPaddingX = 8;
            constexpr int overlayPaddingY = 6;
            const int overlayWidth = std::max(1, width() / 4);
            const QRect textRect(
                overlayMargin + overlayPaddingX,
                overlayMargin + overlayPaddingY,
                std::max(1, overlayWidth - overlayPaddingX * 2),
                std::max(1, height() - (overlayMargin + overlayPaddingY) * 2)
            );
            const QRect textBounds = painter.fontMetrics().boundingRect(
                textRect,
                Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap,
                statusText
            );
            const QRect overlayRect(
                overlayMargin,
                overlayMargin,
                overlayWidth,
                textBounds.height() + overlayPaddingY * 2
            );

            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(80, 80, 80, 150));
            painter.drawRoundedRect(overlayRect, 8.0, 8.0);

            painter.setPen(QColor(230, 230, 230));
            painter.drawText(overlayRect.adjusted(
                    overlayPaddingX,
                    overlayPaddingY,
                    -overlayPaddingX,
                    -overlayPaddingY),
                Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap,
                statusText);
        }

        void mousePressEvent(QMouseEvent *event) override {
            _lastMousePos = event->position().toPoint();
            _owner->updateMouseCoords(mapToOutputPixel(_lastMousePos));

            if (_owner->viewportUsesDirectPick() &&
                event->button() == Qt::LeftButton) {
                _owner->pickAtPixel(mapToOutputPixel(_lastMousePos));
                return;
            }

            const auto mode = effectiveMode();
            const bool panDrag =
                (mode == mandelbrotgui::NavMode::pan &&
                    (event->button() == Qt::LeftButton ||
                        event->button() == Qt::RightButton)) ||
                ((mode == mandelbrotgui::NavMode::zoom ||
                    mode == mandelbrotgui::NavMode::realtimeZoom) &&
                    event->button() == Qt::MiddleButton);
            if (panDrag) {
                _panning = true;
                _dragOrigin = _lastMousePos;
                _panOffset = {};
                _panCommittedOffset = {};
                _selectionRect = {};
                _zoomOutPending = false;
                _zoomOutDragActive = false;
                _zoomOutDragMoved = false;
                _zoomOutPendingCommit = false;
                _zoomOutPreviewScale = 1.0;
                _rtZoomTimer.stop();
                _panRedrawTimer.stop();
                _zoomOutRedrawTimer.stop();
                _panRedrawTimer.start();
                grabMouse();
                setCursor(Qt::ClosedHandCursor);
                update();
                return;
            }

            if (mode == mandelbrotgui::NavMode::realtimeZoom) {
                if (event->button() == Qt::LeftButton ||
                    event->button() == Qt::RightButton) {
                    _rtZoomAnchorPixel = mapToOutputPixel(_lastMousePos);
                    _rtZoomZoomIn = event->button() == Qt::LeftButton;
                    grabMouse();
                    _rtZoomTimer.start();
                    return;
                }
            }

            if (mode == mandelbrotgui::NavMode::zoom) {
                if (event->button() == Qt::LeftButton) {
                    _selectionOrigin = event->position().toPoint();
                    _selectionRect = QRect(_selectionOrigin, QSize(1, 1));
                    _zoomOutPending = false;
                    _zoomOutDragActive = false;
                    _zoomOutDragMoved = false;
                    _zoomOutPendingCommit = false;
                    _zoomOutPreviewScale = 1.0;
                    _zoomOutRedrawTimer.stop();
                    grabMouse();
                    return;
                }

                if (event->button() == Qt::RightButton) {
                    _zoomOutPending = true;
                    _zoomOutDragActive = true;
                    _zoomOutDragMoved = false;
                    _zoomOutDragLastPos = _lastMousePos;
                    _zoomOutPendingCommit = false;
                    _zoomOutPreviewScale = 1.0;
                    _selectionRect = {};
                    _zoomOutRedrawTimer.stop();
                    grabMouse();
                    return;
                }
            }
        }

        void mouseMoveEvent(QMouseEvent *event) override {
            _lastMousePos = event->position().toPoint();
            _owner->updateMouseCoords(mapToOutputPixel(_lastMousePos));

            if (_panning) {
                _panOffset = _lastMousePos - _dragOrigin;
                update();
                return;
            }

            if (_zoomOutDragActive) {
                const QPoint delta = _lastMousePos - _zoomOutDragLastPos;
                _zoomOutDragLastPos = _lastMousePos;
                const int dragMagnitude = std::abs(delta.x()) + std::abs(delta.y());
                if (dragMagnitude > 0) {
                    _zoomOutDragMoved = true;
                    constexpr double kZoomOutDragSensitivity = 0.0025;
                    const double stepScale =
                        std::pow(1.0 + kZoomOutDragSensitivity, dragMagnitude);
                    _zoomOutPreviewScale = std::clamp(
                        _zoomOutPreviewScale * stepScale, 1.0, 64.0);
                    _zoomOutPendingCommit = _zoomOutPreviewScale > 1.0001;
                    if (_zoomOutPendingCommit && !_zoomOutRedrawTimer.isActive()) {
                        _zoomOutRedrawTimer.start();
                    }
                    update();
                }
                return;
            }

            if (!_selectionRect.isNull()) {
                _selectionRect.setBottomRight(_lastMousePos);
                update();
            }
        }

        void mouseReleaseEvent(QMouseEvent *event) override {
            _lastMousePos = event->position().toPoint();
            _owner->updateMouseCoords(mapToOutputPixel(_lastMousePos));

            if (_panning) {
                _panRedrawTimer.stop();
                _panning = false;
                commitPanOffset();
                releaseMouse();
                updateCursor();
                update();
                return;
            }

            if (_rtZoomTimer.isActive()) {
                _rtZoomTimer.stop();
                releaseMouse();
                updateCursor();
                return;
            }

            if (_zoomOutDragActive &&
                (event->button() == Qt::RightButton ||
                    event->button() == Qt::NoButton)) {
                _zoomOutDragActive = false;
                _zoomOutPending = false;
                _zoomOutRedrawTimer.stop();
                if (mouseGrabber() == this) {
                    releaseMouse();
                }
                if (_zoomOutPendingCommit) {
                    commitZoomOutPreview(true);
                } else if (!_zoomOutDragMoved) {
                    _owner->zoomAtPixel(mapToOutputPixel(_lastMousePos), false);
                }
                _zoomOutDragMoved = false;
                _zoomOutPendingCommit = false;
                _zoomOutPreviewScale = 1.0;
                updateCursor();
                update();
                return;
            }

            if (!_selectionRect.isNull()) {
                const bool commitSelection =
                    event->button() == Qt::LeftButton ||
                    event->button() == Qt::NoButton;
                const QRect rect = mapToOutputRect(_selectionRect.normalized());
                _selectionRect = {};
                if (mouseGrabber() == this) {
                    releaseMouse();
                }

                if (commitSelection) {
                    if (rect.width() >= 8 && rect.height() >= 8) {
                        _owner->boxZoom(rect);
                    }
                }

                _zoomOutPending = false;
                updateCursor();
                update();
                return;
            }

            if (_zoomOutPending && event->button() == Qt::RightButton) {
                _zoomOutPending = false;
                _owner->zoomAtPixel(mapToOutputPixel(_lastMousePos), false);
            }
        }

        void mouseDoubleClickEvent(QMouseEvent *event) override {
            if (_owner->viewportUsesDirectPick()) return;
            _owner->pickAtPixel(mapToOutputPixel(event->position()));
        }

        void wheelEvent(QWheelEvent *event) override {
            _lastMousePos = event->position().toPoint();
            _owner->updateMouseCoords(mapToOutputPixel(_lastMousePos));

            if (_panning) {
                event->accept();
                return;
            }

            const auto mode = effectiveMode();
            const bool zoomIn = event->angleDelta().y() > 0;

            if (mode == mandelbrotgui::NavMode::realtimeZoom) {
                const QPoint anchor = _rtZoomAnchorPixel.value_or(
                    mapToOutputPixel(_lastMousePos));
                _rtZoomAnchorPixel = anchor;
                _owner->zoomAtPixel(anchor, zoomIn);
                return;
            }

            if (mode == mandelbrotgui::NavMode::zoom) {
                const bool resizeSelection =
                    !_selectionRect.isNull() &&
                    (event->buttons() & Qt::LeftButton);
                if (!resizeSelection) {
                    event->accept();
                    return;
                }

                const QRect current = _selectionRect.normalized();
                const QPointF center = current.center();
                const double factor = zoomIn ? 0.9 : 1.1;
                const double maxWidth = std::max(2.0, static_cast<double>(width() - 2));
                const double maxHeight = std::max(2.0, static_cast<double>(height() - 2));
                const double nextWidth = std::clamp(current.width() * factor, 2.0, maxWidth);
                const double nextHeight = std::clamp(current.height() * factor, 2.0, maxHeight);
                QRect scaled(
                    static_cast<int>(std::lround(center.x() - nextWidth / 2.0)),
                    static_cast<int>(std::lround(center.y() - nextHeight / 2.0)),
                    std::max(2, static_cast<int>(std::lround(nextWidth))),
                    std::max(2, static_cast<int>(std::lround(nextHeight))));
                const QRect bounds(0, 0, std::max(1, width()), std::max(1, height()));
                scaled = scaled.intersected(bounds);
                if (scaled.width() < 2 || scaled.height() < 2) {
                    event->accept();
                    return;
                }

                _selectionOrigin = scaled.topLeft();
                _selectionRect = QRect(_selectionOrigin, scaled.bottomRight());
                update();
                event->accept();
                return;
            }

            const int delta = zoomIn ? 10 : -10;
            _owner->adjustIterationsBy(delta);
        }

        void keyPressEvent(QKeyEvent *event) override {
            if (event->isAutoRepeat()) return;

            if (event->key() == Qt::Key_Z) {
                _owner->cycleNavMode();
                return;
            }

            if (event->key() == Qt::Key_Space) {
                _spacePan = true;
                setCursor(Qt::OpenHandCursor);
                return;
            }

            if (event->key() == Qt::Key_H) {
                _owner->applyHomeView();
            }
        }

        void keyReleaseEvent(QKeyEvent *event) override {
            if (event->isAutoRepeat()) return;
            if (event->key() != Qt::Key_Space) return;

            _spacePan = false;
            updateCursor();
        }

        void enterEvent(QEnterEvent *) override {
            updateCursor();
        }

        void closeEvent(QCloseEvent *event) override {
            _panRedrawTimer.stop();
            _zoomOutRedrawTimer.stop();
            if (mouseGrabber() == this) {
                releaseMouse();
            }
            QWidget::closeEvent(event);
            _owner->onViewportClosed();
        }

    private:
        QPoint mapToOutputPixel(const QPoint &logicalPoint) const {
            const QSize logicalSize = size();
            const QSize outputSize = _owner->outputSize();
            if (logicalSize.width() <= 0 || logicalSize.height() <= 0) {
                return {};
            }

            const double x = static_cast<double>(logicalPoint.x()) *
                outputSize.width() / std::max(1, logicalSize.width());
            const double y = static_cast<double>(logicalPoint.y()) *
                outputSize.height() / std::max(1, logicalSize.height());

            return {
                std::clamp(static_cast<int>(std::lround(x)), 0,
                    std::max(0, outputSize.width() - 1)),
                std::clamp(static_cast<int>(std::lround(y)), 0,
                    std::max(0, outputSize.height() - 1))
            };
        }

        QPoint mapToOutputPixel(const QPointF &logicalPoint) const {
            return mapToOutputPixel(logicalPoint.toPoint());
        }

        QPoint mapToOutputDelta(const QPoint &logicalDelta) const {
            const QSize logicalSize = size();
            const QSize outputSize = _owner->outputSize();
            if (logicalSize.width() <= 0 || logicalSize.height() <= 0) {
                return {};
            }

            return {
                static_cast<int>(std::lround(
                    static_cast<double>(logicalDelta.x()) *
                    outputSize.width() / std::max(1, logicalSize.width()))),
                static_cast<int>(std::lround(
                    static_cast<double>(logicalDelta.y()) *
                    outputSize.height() / std::max(1, logicalSize.height())))
            };
        }

        QRect mapToOutputRect(const QRect &logicalRect) const {
            const QPoint topLeft = mapToOutputPixel(logicalRect.topLeft());
            const QPoint bottomRight = mapToOutputPixel(logicalRect.bottomRight());
            return QRect(topLeft, bottomRight).normalized();
        }

        mandelbrotgui::NavMode effectiveMode() const {
            return _spacePan ? mandelbrotgui::NavMode::pan : _owner->navMode();
        }

        void updateCursor() {
            switch (effectiveMode()) {
                case mandelbrotgui::NavMode::realtimeZoom:
                case mandelbrotgui::NavMode::zoom:
                    setCursor(Qt::CrossCursor);
                    break;
                case mandelbrotgui::NavMode::pan:
                    setCursor(Qt::OpenHandCursor);
                    break;
            }
        }

        void commitPanOffset() {
            if (!_owner || _panOffset.isNull()) return;

            const QPoint delta = mapToOutputDelta(_panOffset);
            if (delta.isNull()) return;

            _panCommittedOffset += _panOffset;
            _owner->panByPixels(delta);
            _dragOrigin = _lastMousePos;
            _panOffset = {};
            update();
        }

        void commitZoomOutPreview(bool force) {
            if (!_owner) return;
            if (!_zoomOutPendingCommit || _zoomOutPreviewScale <= 1.0001) return;
            if (!force && _owner->renderInFlight()) return;

            const QPoint viewportCenter(width() / 2, height() / 2);
            _owner->scaleAtPixel(
                mapToOutputPixel(viewportCenter),
                _zoomOutPreviewScale,
                !force);
            _zoomOutCommittedScale = std::clamp(
                _zoomOutCommittedScale * _zoomOutPreviewScale, 1.0, 1024.0);
            _zoomOutPreviewScale = 1.0;
            _zoomOutPendingCommit = false;
            update();
        }

        mandelbrotgui *_owner = nullptr;
        QTimer _rtZoomTimer;
        QTimer _panRedrawTimer;
        QTimer _zoomOutRedrawTimer;
        QPoint _lastMousePos;
        QPoint _dragOrigin;
        QPoint _selectionOrigin;
        QPoint _panOffset;
        QPoint _panCommittedOffset;
        QRect _selectionRect;
        std::optional<QPoint> _rtZoomAnchorPixel;
        bool _rtZoomZoomIn = true;
        bool _panning = false;
        bool _spacePan = false;
        bool _zoomOutPending = false;
        bool _zoomOutDragActive = false;
        bool _zoomOutDragMoved = false;
        bool _zoomOutPendingCommit = false;
        QPoint _zoomOutDragLastPos;
        double _zoomOutCommittedScale = 1.0;
        double _zoomOutPreviewScale = 1.0;
    };
}

mandelbrotgui::mandelbrotgui(QWidget *parent)
    : QWidget(parent) {
    buildUi();
    populateControls();
    initializeState();
    connectUi();
    loadSelectedBackend();
    syncControlsFromState();
    updateModeEnablement();
    updateControlWindowSize();

    _viewport = new ViewportWindow(this);
    _viewport->show();
    resizeViewportWindowToImageSize();
    requestRender(true);
}

mandelbrotgui::~mandelbrotgui() {
    _closing = true;
    if (_viewport) {
        _viewport->close();
        _viewport = nullptr;
    }

    finishRenderThread();
}

void mandelbrotgui::buildUi() {
    setWindowFlag(Qt::Window, true);
    setWindowTitle("Control");

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(8, 8, 8, 8);
    outerLayout->setSpacing(4);

    _controlScrollArea = new QScrollArea(this);
    _controlScrollArea->setWidgetResizable(true);
    _controlScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _controlScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    _controlScrollArea->setFrameShape(QFrame::NoFrame);
    _controlScrollArea->verticalScrollBar()->setFixedWidth(kControlScrollBarWidth);
    _controlScrollArea->setStyleSheet(QString(
        "QScrollBar:vertical {"
        " width: %1px;"
        " margin: 0;"
        "}"
        "QScrollBar::handle:vertical {"
        " min-height: 24px;"
        " background: palette(mid);"
        " border-radius: 4px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        " height: 0px;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        " background: transparent;"
        "}").arg(kControlScrollBarWidth));
    outerLayout->addWidget(_controlScrollArea);

    _controlScrollContent = new QWidget(_controlScrollArea);
    _controlScrollArea->setWidget(_controlScrollContent);

    auto *root = new QVBoxLayout(_controlScrollContent);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(3);
    const auto onSectionToggled = [this]() { updateControlWindowSize(); };

    _cpuGroup = new CollapsibleGroupBox("CPU", true, onSectionToggled);
    auto *cpuLayout = new QGridLayout(_cpuGroup);
    cpuLayout->addWidget(new QLabel("Name"), 0, 0);
    _cpuNameEdit = new QLineEdit();
    _cpuNameEdit->setReadOnly(true);
    _cpuNameEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    cpuLayout->addWidget(_cpuNameEdit, 0, 1, 1, 5);
    cpuLayout->addWidget(new QLabel("Cores"), 1, 0);
    _cpuCoresEdit = new QLineEdit();
    _cpuCoresEdit->setReadOnly(true);
    _cpuCoresEdit->setFixedWidth(48);
    cpuLayout->addWidget(_cpuCoresEdit, 1, 1);
    cpuLayout->addWidget(new QLabel("Threads"), 1, 2);
    _cpuThreadsEdit = new QLineEdit();
    _cpuThreadsEdit->setReadOnly(true);
    _cpuThreadsEdit->setFixedWidth(48);
    cpuLayout->addWidget(_cpuThreadsEdit, 1, 3);
    _useThreadsCheckBox = new QCheckBox("Use Threads");
    cpuLayout->addWidget(_useThreadsCheckBox, 1, 4, 1, 2);
    root->addWidget(_cpuGroup);
    static_cast<CollapsibleGroupBox *>(_cpuGroup)->applyExpandedState(
        _cpuGroup->isChecked());

    _renderGroup = new CollapsibleGroupBox("Render", true, onSectionToggled);
    auto *renderLayout = new QGridLayout(_renderGroup);
    renderLayout->addWidget(new QLabel("Variant"), 0, 0);
    _variantCombo = new QComboBox();
    _variantCombo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    _variantCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    _variantCombo->setMinimumContentsLength(14);
    renderLayout->addWidget(_variantCombo, 0, 1, 1, 3);
    renderLayout->addWidget(new QLabel("Iterations"), 1, 0);
    _iterationsSpin = new QSpinBox();
    _iterationsSpin->setRange(0, 1000000000);
    _iterationsSpin->setSpecialValueText("Auto");
    renderLayout->addWidget(_iterationsSpin, 1, 1, 1, 3);
    renderLayout->addWidget(new QLabel("Color"), 2, 0);
    _colorMethodCombo = new QComboBox();
    _colorMethodCombo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    _colorMethodCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    _colorMethodCombo->setMinimumContentsLength(12);
    renderLayout->addWidget(_colorMethodCombo, 2, 1, 1, 3);
    renderLayout->addWidget(new QLabel("Fractal"), 3, 0);
    _fractalCombo = new QComboBox();
    _fractalCombo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    _fractalCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    _fractalCombo->setMinimumContentsLength(12);
    renderLayout->addWidget(_fractalCombo, 3, 1, 1, 3);
    _juliaCheck = new QCheckBox("Julia");
    _inverseCheck = new QCheckBox("Inverse");
    renderLayout->addWidget(_juliaCheck, 4, 1);
    renderLayout->addWidget(_inverseCheck, 4, 2);
    root->addWidget(_renderGroup);
    static_cast<CollapsibleGroupBox *>(_renderGroup)->applyExpandedState(
        _renderGroup->isChecked());

    _infoGroup = new CollapsibleGroupBox("Info", true, onSectionToggled);
    auto *infoLayout = new QFormLayout(_infoGroup);
    _infoRealEdit = new QLineEdit();
    _infoRealEdit->setReadOnly(true);
    _infoRealEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    infoLayout->addRow("Real", _infoRealEdit);
    _infoImagEdit = new QLineEdit();
    _infoImagEdit->setReadOnly(true);
    _infoImagEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    infoLayout->addRow("Imag", _infoImagEdit);
    _infoZoomEdit = new QLineEdit();
    _infoZoomEdit->setReadOnly(true);
    _infoZoomEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    infoLayout->addRow("Zoom", _infoZoomEdit);
    auto *pointButtonsLayout = new QHBoxLayout();
    pointButtonsLayout->setContentsMargins(0, 0, 0, 0);
    _savePointButton = new QPushButton("Save View");
    _loadPointButton = new QPushButton("Load View");
    stabilizePushButton(_savePointButton);
    stabilizePushButton(_loadPointButton);
    pointButtonsLayout->addWidget(_savePointButton);
    pointButtonsLayout->addWidget(_loadPointButton);
    infoLayout->addRow(QString(), pointButtonsLayout);
    root->addWidget(_infoGroup);
    static_cast<CollapsibleGroupBox *>(_infoGroup)->applyExpandedState(
        _infoGroup->isChecked());

    _viewportGroup = new CollapsibleGroupBox("Viewport", true, onSectionToggled);
    auto *navLayout = new QGridLayout(_viewportGroup);
    navLayout->setColumnStretch(0, 0);
    navLayout->setColumnStretch(1, 1);
    navLayout->setColumnStretch(2, 0);
    navLayout->setHorizontalSpacing(8);
    navLayout->addWidget(new QLabel("Mode"), 0, 0);
    _navModeCombo = new QComboBox();
    _navModeCombo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    _navModeCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    _navModeCombo->setMinimumContentsLength(10);
    navLayout->addWidget(_navModeCombo, 0, 1, 1, 2);
    navLayout->addWidget(new QLabel("Pick"), 1, 0);
    _pickTargetCombo = new QComboBox();
    _pickTargetCombo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    _pickTargetCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    _pickTargetCombo->setMinimumContentsLength(10);
    navLayout->addWidget(_pickTargetCombo, 1, 1, 1, 2);

    navLayout->addWidget(new QLabel("Pan Rate"), 2, 0);
    _panRateSlider = new QSlider(Qt::Horizontal);
    _panRateSlider->setRange(0, 20);
    _panRateSlider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    _panRateSpin = new QSpinBox();
    _panRateSpin->setRange(0, 20);
    _panRateSpin->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    _panRateSpin->setFixedWidth(72);
    navLayout->addWidget(_panRateSlider, 2, 1);
    navLayout->addWidget(_panRateSpin, 2, 2);

    navLayout->addWidget(new QLabel("Zoom Rate"), 3, 0);
    _zoomRateSlider = new QSlider(Qt::Horizontal);
    _zoomRateSlider->setRange(0, 20);
    _zoomRateSlider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    _zoomRateSpin = new QSpinBox();
    _zoomRateSpin->setRange(0, 20);
    _zoomRateSpin->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    _zoomRateSpin->setFixedWidth(72);
    navLayout->addWidget(_zoomRateSlider, 3, 1);
    navLayout->addWidget(_zoomRateSpin, 3, 2);

    navLayout->addWidget(new QLabel("Exponent"), 4, 0);
    _exponentSlider = new QSlider(Qt::Horizontal);
    _exponentSlider->setRange(101, 1600);
    _exponentSlider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    _exponentSpin = new QDoubleSpinBox();
    _exponentSpin->setRange(1.01, 16.0);
    _exponentSpin->setDecimals(2);
    _exponentSpin->setSingleStep(0.01);
    _exponentSpin->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    _exponentSpin->setFixedWidth(72);
    navLayout->addWidget(_exponentSlider, 4, 1);
    navLayout->addWidget(_exponentSpin, 4, 2);
    root->addWidget(_viewportGroup);
    static_cast<CollapsibleGroupBox *>(_viewportGroup)->applyExpandedState(
        _viewportGroup->isChecked());

    _sineGroup = new CollapsibleGroupBox("Sine Color", false, onSectionToggled);
    auto *sineLayout = new QGridLayout(_sineGroup);
    sineLayout->setHorizontalSpacing(6);
    sineLayout->setColumnStretch(1, 1);
    sineLayout->setColumnStretch(3, 1);
    sineLayout->setColumnMinimumWidth(4, 6);
    sineLayout->addWidget(new QLabel("Name"), 0, 0);
    _sineCombo = new QComboBox();
    _sineCombo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    _sineCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    _sineCombo->setMinimumContentsLength(10);
    sineLayout->addWidget(_sineCombo, 0, 1, 1, 3);
    _randomizeSineButton = new QPushButton("Randomize RGB");
    stabilizePushButton(_randomizeSineButton);
    _randomizeSineButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    sineLayout->addWidget(_randomizeSineButton, 0, 5);
    _importSineButton = new QPushButton("Import");
    stabilizePushButton(_importSineButton);
    _importSineButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    _saveSineButton = new QPushButton("Save");
    stabilizePushButton(_saveSineButton);
    _saveSineButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    sineLayout->addWidget(new QLabel("R"), 1, 0);
    _freqRSpin = new QDoubleSpinBox();
    _freqRSpin->setRange(0.0, 1000.0);
    _freqRSpin->setDecimals(4);
    _freqRSpin->setSingleStep(0.01);
    sineLayout->addWidget(_freqRSpin, 1, 1);
    sineLayout->addWidget(new QLabel("G"), 1, 2);
    _freqGSpin = new QDoubleSpinBox();
    _freqGSpin->setRange(0.0, 1000.0);
    _freqGSpin->setDecimals(4);
    _freqGSpin->setSingleStep(0.01);
    sineLayout->addWidget(_freqGSpin, 1, 3);
    sineLayout->addWidget(_importSineButton, 1, 5);
    sineLayout->addWidget(new QLabel("B"), 2, 0);
    _freqBSpin = new QDoubleSpinBox();
    _freqBSpin->setRange(0.0, 1000.0);
    _freqBSpin->setDecimals(4);
    _freqBSpin->setSingleStep(0.01);
    sineLayout->addWidget(_freqBSpin, 2, 1);
    sineLayout->addWidget(new QLabel("Mult"), 2, 2);
    _freqMultSpin = new QDoubleSpinBox();
    _freqMultSpin->setRange(0.0001, 1000.0);
    _freqMultSpin->setDecimals(4);
    _freqMultSpin->setSingleStep(0.01);
    sineLayout->addWidget(_freqMultSpin, 2, 3);
    sineLayout->addWidget(_saveSineButton, 2, 5);
    const int compactComboWidth = _sineCombo->sizeHint().width();
    const int compactSpinWidth = _freqMultSpin->sizeHint().width();
    auto *sinePreview = new SinePreviewWidget();
    sinePreview->onRangeChanged = [this](double, double) { updateSinePreview(); };
    _sinePreview = sinePreview;
    sineLayout->addWidget(_sinePreview, 3, 0, 1, 6);
    root->addWidget(_sineGroup);
    static_cast<CollapsibleGroupBox *>(_sineGroup)->applyExpandedState(
        _sineGroup->isChecked());

    _paletteGroup = new CollapsibleGroupBox("Palette", false, onSectionToggled);
    auto *paletteLayout = new QGridLayout(_paletteGroup);
    paletteLayout->setHorizontalSpacing(6);
    paletteLayout->setColumnStretch(1, 1);
    paletteLayout->setColumnStretch(3, 1);
    paletteLayout->setColumnMinimumWidth(4, 6);
    paletteLayout->addWidget(new QLabel("Name"), 0, 0);
    _paletteCombo = new QComboBox();
    _paletteCombo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    _paletteCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    _paletteCombo->setMinimumContentsLength(10);
    _paletteCombo->setFixedWidth(compactComboWidth);
    paletteLayout->addWidget(_paletteCombo, 0, 1, 1, 3);
    _paletteEditorButton = new QPushButton("Edit");
    stabilizePushButton(_paletteEditorButton);
    _paletteEditorButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    paletteLayout->addWidget(_paletteEditorButton, 0, 5);
    paletteLayout->addWidget(new QLabel("Length"), 1, 0);
    _paletteLengthSpin = new QDoubleSpinBox();
    _paletteLengthSpin->setRange(0.0001, 1000000.0);
    _paletteLengthSpin->setDecimals(4);
    _paletteLengthSpin->setFixedWidth(compactSpinWidth);
    paletteLayout->addWidget(_paletteLengthSpin, 1, 1);
    paletteLayout->addWidget(new QLabel("Offset"), 1, 2);
    _paletteOffsetSpin = new QDoubleSpinBox();
    _paletteOffsetSpin->setRange(0.0, 1000000.0);
    _paletteOffsetSpin->setDecimals(4);
    _paletteOffsetSpin->setFixedWidth(compactSpinWidth);
    paletteLayout->addWidget(_paletteOffsetSpin, 1, 3);
    _palettePreviewLabel = new PalettePreviewWidget();
    paletteLayout->addWidget(_palettePreviewLabel, 2, 0, 1, 6);
    root->addWidget(_paletteGroup);
    static_cast<CollapsibleGroupBox *>(_paletteGroup)->applyExpandedState(
        _paletteGroup->isChecked());

    _lightGroup = new CollapsibleGroupBox("Light", false, onSectionToggled);
    auto *lightLayout = new QFormLayout(_lightGroup);
    _lightRealEdit = new QLineEdit();
    _lightRealEdit->setReadOnly(true);
    _lightRealEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    lightLayout->addRow("Real", _lightRealEdit);
    _lightImagEdit = new QLineEdit();
    _lightImagEdit->setReadOnly(true);
    _lightImagEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    lightLayout->addRow("Imag", _lightImagEdit);
    _lightColorButton = new QPushButton();
    stabilizePushButton(_lightColorButton);
    _lightColorButton->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    lightLayout->addRow("Color", _lightColorButton);
    root->addWidget(_lightGroup);
    static_cast<CollapsibleGroupBox *>(_lightGroup)->applyExpandedState(
        _lightGroup->isChecked());

    auto *outputGroup = new CollapsibleGroupBox("Image", true, onSectionToggled);
    auto *outputLayout = new QGridLayout(outputGroup);
    outputLayout->setColumnStretch(0, 0);
    outputLayout->setColumnStretch(1, 0);
    outputLayout->setColumnStretch(2, 0);
    outputLayout->setColumnStretch(3, 0);
    outputLayout->setColumnStretch(4, 0);
    outputLayout->setColumnStretch(5, 0);
    outputLayout->setColumnStretch(6, 1);
    outputLayout->setHorizontalSpacing(8);
    outputLayout->addWidget(new QLabel("W"), 0, 0);
    _outputWidthSpin = new QSpinBox();
    _outputWidthSpin->setRange(1, 32768);
    _outputWidthSpin->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    _outputWidthSpin->setFixedWidth(96);
    outputLayout->addWidget(_outputWidthSpin, 0, 1);
    auto *timesLabel = new QLabel("x");
    timesLabel->setAlignment(Qt::AlignCenter);
    outputLayout->addWidget(timesLabel, 0, 2);
    outputLayout->addWidget(new QLabel("H"), 0, 3);
    _outputHeightSpin = new QSpinBox();
    _outputHeightSpin->setRange(1, 32768);
    _outputHeightSpin->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    _outputHeightSpin->setFixedWidth(96);
    outputLayout->addWidget(_outputHeightSpin, 0, 4);
    _resizeButton = new QPushButton("Resize");
    stabilizePushButton(_resizeButton);
    outputLayout->addWidget(_resizeButton, 0, 5);
    outputLayout->addWidget(new QLabel("AA"), 1, 0);
    _aaSpin = new QSpinBox();
    _aaSpin->setRange(1, 16);
    outputLayout->addWidget(_aaSpin, 1, 1, 1, 2);
    _preserveRatioCheck = new QCheckBox("Keep Aspect Ratio");
    outputLayout->addWidget(_preserveRatioCheck, 1, 3, 1, 3);
    _imageMemoryLabel = new QLabel("Render: -  Output: -");
    _imageMemoryLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    _imageMemoryLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    outputLayout->addWidget(_imageMemoryLabel, 2, 0, 1, 7);
    root->addWidget(outputGroup);
    outputGroup->applyExpandedState(outputGroup->isChecked());

    root->addStretch(1);

    auto *buttonPanel = new QWidget(this);
    auto *buttonRow = new QGridLayout(buttonPanel);
    buttonRow->setContentsMargins(0, 0, 0, 0);
    _calculateButton = new QPushButton("Calculate");
    _homeButton = new QPushButton("Home");
    _zoomButton = new QPushButton("Zoom");
    _saveButton = new QPushButton("Save");
    stabilizePushButton(_calculateButton);
    stabilizePushButton(_homeButton);
    stabilizePushButton(_zoomButton);
    stabilizePushButton(_saveButton);
    buttonRow->addWidget(_calculateButton, 0, 0);
    buttonRow->addWidget(_homeButton, 0, 1);
    buttonRow->addWidget(_zoomButton, 1, 0);
    buttonRow->addWidget(_saveButton, 1, 1);
    outerLayout->addWidget(buttonPanel);

    auto *statusPanel = new QWidget(this);
    auto *statusRow = new QHBoxLayout(statusPanel);
    statusRow->setContentsMargins(0, 0, 0, 0);
    _progressLabel = new QLabel("0%");
    _progressLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    _progressLabel->setFixedWidth(40);
    _progressBar = new QProgressBar();
    _progressBar->setRange(0, 100);
    _progressBar->setValue(0);
    _progressBar->setTextVisible(false);
    _progressBar->setFixedWidth(100);
    _statusRightLabel = new QLabel();
    _statusRightLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    _statusRightLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    statusRow->addWidget(_progressLabel);
    statusRow->addWidget(_progressBar);
    statusRow->addSpacing(8);
    statusRow->addWidget(_statusRightLabel, 1);
    outerLayout->addWidget(statusPanel);
}

void mandelbrotgui::populateControls() {
    const QString prefix = QString::fromStdString(currentPrefix());
    _variantCombo->addItems({
        prefix + " - FloatScalar",
        prefix + " - DoubleScalar",
        prefix + " - FloatAVX2",
        prefix + " - DoubleAVX2",
        prefix + " - MPFR"
        });

    _colorMethodCombo->addItems({
        "Iterations",
        "Smooth Iterations",
        "Palette",
        "Light"
    });
    _fractalCombo->addItems({
        "Mandelbrot",
        "Perpendicular",
        "Burning Ship"
    });
    _navModeCombo->addItems({
        "Realtime Zoom",
        "Box Zoom",
        "Pan"
    });
    _pickTargetCombo->addItems({
        "Zoom Point",
        "Seed Point",
        "Light Point"
    });
}

void mandelbrotgui::initializeState() {
    const int floatAvx2Index = _variantCombo->findText("FloatAVX2",
        Qt::MatchContains);
    _state.variant = floatAvx2Index >= 0 ?
        _variantCombo->itemText(floatAvx2Index) :
        _variantCombo->itemText(0);
    _state.iterations = 0;
    _state.sineName = kDefaultSineName;
    _state.sinePalette = defaultSinePalette();
    refreshSineList(_state.sineName);
    if (_sineCombo && _sineCombo->count() > 0) {
        QString sineError;
        QString initialName = _state.sineName;
        if (_sineCombo->findText(initialName, Qt::MatchFixedString) < 0) {
            initialName = _sineCombo->itemText(0);
        }
        if (!initialName.isEmpty()) {
            const std::filesystem::path sourcePath = sineFilePath(initialName);
            if (std::filesystem::exists(sourcePath)) {
                Backend::SinePaletteConfig loaded;
                if (loadSineFromPath(sourcePath, loaded, sineError)) {
                    _state.sineName = initialName;
                    _state.sinePalette = loaded;
                }
            } else if (initialName.compare(kDefaultSineName, Qt::CaseInsensitive) == 0) {
                _state.sineName = initialName;
                _state.sinePalette = defaultSinePalette();
            }
        }
    }

    _state.paletteName = kDefaultPaletteName;
    _state.palette = defaultPalette();
    refreshPaletteList(_state.paletteName);
    if (_paletteCombo && _paletteCombo->count() > 0) {
        QString paletteError;
        QString initialName = _state.paletteName;
        if (_paletteCombo->findText(initialName, Qt::MatchFixedString) < 0) {
            initialName = _paletteCombo->itemText(0);
        }
        if (!initialName.isEmpty()) {
            loadPaletteByName(initialName, false, &paletteError);
        }
    }

    const CPUInfo cpu = queryCPUInfo();
    _cpuNameEdit->setText(QString::fromStdString(cpu.name));
    if (cpu.cores > 0) _cpuCoresEdit->setText(QString::number(cpu.cores));
    if (cpu.threads > 0) _cpuThreadsEdit->setText(QString::number(cpu.threads));

    _statusText = "Ready";
    _mouseText = "Mouse: -";
    _viewportRenderTimeText.clear();
    _imageMemoryText = "Render: -  Output: -";
    syncPointTextFromState();
}

void mandelbrotgui::connectUi() {
    QObject::connect(_variantCombo, &QComboBox::currentTextChanged,
        this, [this](const QString &text) {
            _state.variant = text;
            loadSelectedBackend();
        });

    QObject::connect(_useThreadsCheckBox, &QCheckBox::toggled,
        this, [this](bool checked) {
            _state.useThreads = checked;
        });

    auto requestFromControls = [this]() {
        syncStateFromControls();
        updateSinePreview();
        requestRender();
    };

    QObject::connect(_iterationsSpin, QOverload<int>::of(&QSpinBox::valueChanged),
        this, [requestFromControls](int) { requestFromControls(); });
    QObject::connect(_colorMethodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int) {
            syncStateFromControls();
            updateModeEnablement();
            requestRender();
        });
    QObject::connect(_fractalCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [requestFromControls](int) { requestFromControls(); });
    QObject::connect(_juliaCheck, &QCheckBox::toggled,
        this, [requestFromControls](bool) { requestFromControls(); });
    QObject::connect(_inverseCheck, &QCheckBox::toggled,
        this, [requestFromControls](bool) { requestFromControls(); });
    QObject::connect(_aaSpin, QOverload<int>::of(&QSpinBox::valueChanged),
        this, [requestFromControls](int) { requestFromControls(); });

    QObject::connect(_navModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int index) { updateNavMode(index); });
    QObject::connect(_pickTargetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int index) { updateSelectionTarget(index); });

    QObject::connect(_panRateSlider, &QSlider::valueChanged,
        this, [this](int value) {
            _panRateSpin->blockSignals(true);
            _panRateSpin->setValue(value);
            _panRateSpin->blockSignals(false);
            _state.panRate = value;
        });
    QObject::connect(_panRateSpin, QOverload<int>::of(&QSpinBox::valueChanged),
        this, [this](int value) { _panRateSlider->setValue(value); });

    QObject::connect(_zoomRateSlider, &QSlider::valueChanged,
        this, [this](int value) {
            _zoomRateSpin->blockSignals(true);
            _zoomRateSpin->setValue(value);
            _zoomRateSpin->blockSignals(false);
            _state.zoomRate = value;
        });
    QObject::connect(_zoomRateSpin, QOverload<int>::of(&QSpinBox::valueChanged),
        this, [this](int value) { _zoomRateSlider->setValue(value); });

    QObject::connect(_exponentSlider, &QSlider::valueChanged,
        this, [this](int value) {
            const double exponent = value / 100.0;
            _exponentSpin->blockSignals(true);
            _exponentSpin->setValue(exponent);
            _exponentSpin->blockSignals(false);
            _state.exponent = exponent;
            requestRender();
        });
    QObject::connect(_exponentSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, [this](double value) {
            _exponentSlider->blockSignals(true);
            _exponentSlider->setValue(static_cast<int>(std::round(value * 100.0)));
            _exponentSlider->blockSignals(false);
            _state.exponent = value;
            requestRender();
        });
    QObject::connect(_sineCombo, &QComboBox::currentTextChanged,
        this, [this](const QString &name) {
            if (name.isEmpty()) return;

            QString errorMessage;
            if (!loadSineByName(name, true, &errorMessage) &&
                !errorMessage.isEmpty()) {
                QMessageBox::warning(this, "Sine Color", errorMessage);
                refreshSineList(_state.sineName);
            }
        });
    QObject::connect(_freqRSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, [requestFromControls](double) { requestFromControls(); });
    QObject::connect(_freqGSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, [requestFromControls](double) { requestFromControls(); });
    QObject::connect(_freqBSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, [requestFromControls](double) { requestFromControls(); });
    QObject::connect(_freqMultSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, [requestFromControls](double) { requestFromControls(); });
    QObject::connect(_randomizeSineButton, &QPushButton::clicked,
        this, [this]() {
            auto randomFrequency = []() {
                constexpr double minValue = 0.5;
                constexpr double maxValue = 1.5;
                const double value = minValue +
                    (maxValue - minValue) * QRandomGenerator::global()->generateDouble();
                return std::round(value * 10000.0) / 10000.0;
            };

            const QSignalBlocker rBlocker(_freqRSpin);
            const QSignalBlocker gBlocker(_freqGSpin);
            const QSignalBlocker bBlocker(_freqBSpin);
            _freqRSpin->setValue(randomFrequency());
            _freqGSpin->setValue(randomFrequency());
            _freqBSpin->setValue(randomFrequency());

            syncStateFromControls();
            updateSinePreview();
            requestRender();
        });
    QObject::connect(_saveSineButton, &QPushButton::clicked,
        this, [this]() { saveSine(); });
    QObject::connect(_importSineButton, &QPushButton::clicked,
        this, [this]() { importSine(); });
    QObject::connect(_savePointButton, &QPushButton::clicked,
        this, [this]() { savePointView(); });
    QObject::connect(_loadPointButton, &QPushButton::clicked,
        this, [this]() { loadPointView(); });

    QObject::connect(_paletteCombo, &QComboBox::currentTextChanged,
        this, [this](const QString &name) {
            if (name.isEmpty()) return;

            QString errorMessage;
            if (!loadPaletteByName(name, true, &errorMessage) &&
                !errorMessage.isEmpty()) {
                QMessageBox::warning(this, "Palette", errorMessage);
                refreshPaletteList(_state.paletteName);
            }
        });
    QObject::connect(_paletteLengthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, [this](double value) {
            _state.palette.totalLength = static_cast<float>(value);
            _state.palette = stopsToConfig(
                configToStops(_state.palette),
                _state.palette.totalLength,
                _state.palette.offset,
                _state.palette.blendEnds
            );
            _palettePreviewDirty = true;
            updateViewport();
            requestRender();
        });
    QObject::connect(_paletteOffsetSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, [this](double value) {
            _state.palette.offset = static_cast<float>(value);
            _palettePreviewDirty = true;
            updateViewport();
            requestRender();
        });
    QObject::connect(_paletteEditorButton, &QPushButton::clicked,
        this, [this]() { openPaletteEditor(); });
    QObject::connect(_lightColorButton, &QPushButton::clicked,
        this, [this]() {
            const QColor initial = lightColorToQColor(_state.lightColor);
            const QColor color = QColorDialog::getColor(
                initial, this, "Light Color");
            if (!color.isValid()) return;

            _state.lightColor = lightColorFromQColor(color);
            updateLightColorButton();
            requestRender();
        });

    QObject::connect(_outputWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged),
        this, [this](int) { updateAspectLinkedSizes(true); });
    QObject::connect(_outputHeightSpin, QOverload<int>::of(&QSpinBox::valueChanged),
        this, [this](int) { updateAspectLinkedSizes(false); });
    QObject::connect(_preserveRatioCheck, &QCheckBox::toggled,
        this, [this](bool checked) { _state.preserveRatio = checked; });

    QObject::connect(_calculateButton, &QPushButton::clicked,
        this, [this]() {
            syncStateFromControls();
            requestRender(true);
        });
    QObject::connect(_homeButton, &QPushButton::clicked,
        this, [this]() { applyHomeView(); });
    QObject::connect(_zoomButton, &QPushButton::clicked,
        this, [this]() {
            if (_viewport) {
                const QSize size = outputSize();
                zoomAtPixel(QPoint(size.width() / 2, size.height() / 2), true);
            }
        });
    QObject::connect(_saveButton, &QPushButton::clicked,
        this, [this]() { saveImage(); });
    QObject::connect(_resizeButton, &QPushButton::clicked,
        this, [this]() { resizeViewportImage(); });
}

void mandelbrotgui::loadSelectedBackend() {
    finishRenderThread();
    _backend.reset();
    _previewBackend.reset();

    std::string error;
    _backend = loadBackendModule(executableDir(),
        _state.variant.toStdString(), error);
    if (_backend && _backend.session) {
        std::string previewError;
        _previewBackend = loadBackendModule(executableDir(),
            _state.variant.toStdString(), previewError);
        bindBackendCallbacks();
        startRenderWorker();
        updateViewport();
        return;
    }

    QMessageBox::warning(this, "Backend",
        QString::fromStdString(error.empty() ?
            "Failed to load backend." : error));
}

void mandelbrotgui::bindBackendCallbacks() {
    _callbacks = {};

    _callbacks.onProgress = [this](const Backend::ProgressEvent &event) {
        const uint64_t renderId = _callbackRenderRequestId.load(std::memory_order_acquire);
        const QString progress = QString("%1%").arg(event.percentage);

        QMetaObject::invokeMethod(this, [this, progress, event, renderId]() {
            if (renderId != _latestRenderRequestId) return;

            _progressText = progress;
            _progressValue = std::clamp(event.percentage, 0, 100);
            _progressActive = !event.completed;
            updateStatusLabels();
            if (_viewport) _viewport->update();
        });
    };

    _callbacks.onImage = [this](const Backend::ImageEvent &event) {
        if (event.kind == Backend::ImageEventKind::allocated) {
            const QString imageMemoryText = formatImageMemoryText(event);

            QMetaObject::invokeMethod(this, [this, imageMemoryText]() {
                _imageMemoryText = imageMemoryText;
                updateStatusLabels();
                if (_viewport) _viewport->update();
            });
            return;
        }

        if (event.kind != Backend::ImageEventKind::saved || !event.path) return;
        const QString path = QString::fromUtf8(event.path);

        QMetaObject::invokeMethod(this, [this, path]() {
            _statusText = QString("Saved: %1").arg(path);
            updateStatusLabels();
            if (_viewport) _viewport->update();
        });
    };

    _callbacks.onInfo = [this](const Backend::InfoEvent &event) {
        const uint64_t renderId = _callbackRenderRequestId.load(std::memory_order_acquire);
        const QString text = QString("Iterations: %1 | %2 GI/s")
            .arg(QString::fromStdString(
                FormatUtil::formatNumber(event.totalIterations)))
            .arg(QString::number(event.opsPerSecond, 'f', 2));

        QMetaObject::invokeMethod(this, [this, text, renderId]() {
            if (renderId != _latestRenderRequestId) return;

            _statusText = text;
            updateStatusLabels();
            if (_viewport) _viewport->update();
        });
    };

    _callbacks.onDebug = [this](const Backend::DebugEvent &event) {
        if (!event.message) return;
        const QString message = QString::fromUtf8(event.message);

        QMetaObject::invokeMethod(this, [this, message]() {
            _statusText = message;
            updateStatusLabels();
            if (_viewport) _viewport->update();
        });
    };

    _backend.session->setCallbacks(_callbacks);
}

void mandelbrotgui::startRenderWorker() {
    if (_renderThread.joinable()) return;

    {
        const std::scoped_lock lock(_renderMutex);
        _renderStopRequested = false;
        _queuedRenderRequest.reset();
    }

    _renderThread = std::thread([this]() {
        for (;;) {
            RenderRequest request;

            {
                std::unique_lock lock(_renderMutex);
                _renderCv.wait(lock, [this]() {
                    return _renderStopRequested || _queuedRenderRequest.has_value();
                    });

                if (_renderStopRequested) break;

                request = std::move(*_queuedRenderRequest);
                _queuedRenderRequest.reset();
            }

            _callbackRenderRequestId.store(request.id, std::memory_order_release);

            QMetaObject::invokeMethod(this, [this, requestId = request.id]() {
                if (requestId != _latestRenderRequestId) return;

                _renderInFlight = true;
                _progressActive = true;
                _progressValue = 0;
                _progressText = "Rendering";
                updateStatusLabels();
            });

            QString error;
            if (!applyStateToSession(
                    request.state,
                    request.pointRealText,
                    request.pointImagText,
                    request.pickAction,
                    error)) {
                {
                    const std::scoped_lock lock(_renderMutex);
                    _queuedRenderRequest.reset();
                }

                QMetaObject::invokeMethod(this, [this, error, requestId = request.id]() {
                    if (requestId != _latestRenderRequestId) return;

                    _renderInFlight = false;
                    _progressActive = false;
                    _progressValue = 0;
                    _progressText.clear();
                    handleRenderFailure(error);
                    updateStatusLabels();
                });
                continue;
            }

            const auto renderStart = std::chrono::steady_clock::now();
            const Backend::Status status = _backend.session->render();
            const auto renderEnd = std::chrono::steady_clock::now();
            const auto renderElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                renderEnd - renderStart);
            const qint64 renderMs = std::max<qint64>(1, renderElapsed.count());
            const double renderFps = 1000.0 / static_cast<double>(renderMs);

            if (!status) {
                const QString message = QString::fromStdString(status.message);
                {
                    const std::scoped_lock lock(_renderMutex);
                    _queuedRenderRequest.reset();
                }

                QMetaObject::invokeMethod(this, [this, message, requestId = request.id]() {
                    if (requestId != _latestRenderRequestId) return;

                    _renderInFlight = false;
                    _progressActive = false;
                    _progressValue = 0;
                    _progressText.clear();
                    handleRenderFailure(message);
                    updateStatusLabels();
                });
                continue;
            }

            const Backend::ImageView view = _backend.session->imageView();
            const QImage image = imageViewToImage(view);

            QMetaObject::invokeMethod(this, [this, image, renderFps, renderMs, requestId = request.id]() {
                if (requestId != _latestRenderRequestId) return;

                _renderInFlight = false;
                _progressActive = false;
                _progressValue = 0;
                _progressText.clear();
                _lastRenderFailureMessage.clear();
                _viewportFpsText = QString("FPS %1")
                    .arg(QString::number(renderFps, 'f', 1));
                _viewportRenderTimeText = QString::fromStdString(
                    FormatUtil::formatDuration(renderMs));
                updatePreview(image);
                updateStatusLabels();
            });
        }
    });
}

bool mandelbrotgui::ensureBackendReady(QString &errorMessage) const {
    if (_backend && _backend.session) return true;

    errorMessage = "Backend not loaded.";
    return false;
}

bool mandelbrotgui::applyStateToSession(const UiState &state,
    const QString &pointRealText,
    const QString &pointImagText,
    const std::optional<PendingPickAction> &pickAction,
    QString &errorMessage) {
    if (!ensureBackendReady(errorMessage)) return false;

    auto failIfNeeded = [&errorMessage](const Backend::Status &status) {
        if (status) return false;
        errorMessage = QString::fromStdString(status.message);
        return true;
    };

    if (failIfNeeded(_backend.session->setImageSize(
            state.outputWidth, state.outputHeight, state.aaPixels)))
        return false;
    _backend.session->setUseThreads(state.useThreads);
    if (failIfNeeded(_backend.session->setZoom(
            state.iterations,
            static_cast<float>(clampGuiZoom(state.zoom)))))
        return false;
    if (failIfNeeded(_backend.session->setPoint(
            pointRealText.toStdString(),
            pointImagText.toStdString())))
        return false;
    if (failIfNeeded(_backend.session->setSeed(
            stateToString(state.seed.x()).toStdString(),
            stateToString(state.seed.y()).toStdString())))
        return false;
    if (failIfNeeded(_backend.session->setFractalType(state.fractalType)))
        return false;
    _backend.session->setFractalMode(state.julia, state.inverse);
    if (failIfNeeded(_backend.session->setFractalExponent(
            stateToString(state.exponent).toStdString())))
        return false;
    if (failIfNeeded(_backend.session->setColorMethod(state.colorMethod)))
        return false;
    if (failIfNeeded(_backend.session->setSinePalette(state.sinePalette)))
        return false;
    if (failIfNeeded(_backend.session->setColorPalette(state.palette)))
        return false;
    if (failIfNeeded(_backend.session->setLight(
            static_cast<float>(state.light.x()),
            static_cast<float>(state.light.y()))))
        return false;
    if (failIfNeeded(_backend.session->setLightColor(state.lightColor)))
        return false;

    if (pickAction) {
        Backend::Status status;
        switch (pickAction->target) {
            case SelectionTarget::zoomPoint:
                status = _backend.session->setPoint(
                    pickAction->pixel.x(), pickAction->pixel.y());
                break;
            case SelectionTarget::seedPoint:
                status = _backend.session->setSeed(
                    pickAction->pixel.x(), pickAction->pixel.y());
                break;
            case SelectionTarget::lightPoint:
                status = _backend.session->setLight(
                    pickAction->pixel.x(), pickAction->pixel.y());
                break;
        }

        if (failIfNeeded(status)) return false;
    }

    return true;
}

void mandelbrotgui::finishRenderThread() {
    _latestRenderRequestId++;
    _callbackRenderRequestId.store(0, std::memory_order_release);

    {
        const std::scoped_lock lock(_renderMutex);
        _renderStopRequested = true;
        _queuedRenderRequest.reset();
    }
    _renderCv.notify_all();

    if (_renderThread.joinable()) {
        _renderThread.join();
    }

    {
        const std::scoped_lock lock(_renderMutex);
        _renderStopRequested = false;
        _queuedRenderRequest.reset();
    }

    _renderInFlight = false;
    _progressActive = false;
    _progressValue = 0;
    _progressText.clear();
}

void mandelbrotgui::requestRender(bool force) {
    (void)force;
    syncStateFromControls();
    _state.zoom = clampGuiZoom(_state.zoom);
    syncStateReadouts();
    if (!_backend || !_backend.session) return;

    const auto pickAction = _pendingPickAction;
    _pendingPickAction.reset();
    const uint64_t requestId = ++_latestRenderRequestId;

    {
        const std::scoped_lock lock(_renderMutex);
        _queuedRenderRequest = RenderRequest{
            .state = _state,
            .pointRealText = _pointRealText,
            .pointImagText = _pointImagText,
            .pickAction = pickAction,
            .id = requestId
        };
    }

    if (!_renderInFlight) {
        _progressActive = true;
        _progressValue = 0;
        _progressText = "Rendering";
        updateStatusLabels();
    }

    _renderCv.notify_one();
}

void mandelbrotgui::applyHomeView() {
    _state.iterations = 0;
    _state.zoom = -0.65;
    _state.exponent = 2.0;
    _state.point = QPointF(0.0, 0.0);
    _state.seed = QPointF(0.0, 0.0);
    _state.light = QPointF(1.0, 1.0);
    syncPointTextFromState();
    syncControlsFromState();
    requestRender(true);
}

void mandelbrotgui::scaleAtPixel(
    const QPoint &pixel,
    double scaleMultiplier,
    bool realtimeStep) {
    if (realtimeStep && _renderInFlight) return;
    if (!(scaleMultiplier > 0.0) || !std::isfinite(scaleMultiplier)) return;

    const QPoint clampedPixel = clampPixelToOutput(pixel);
    const double nextZoom = clampGuiZoom(_state.zoom - std::log10(scaleMultiplier));
    if (nextZoom == _state.zoom) return;

    QString real;
    QString imag;
    QString errorMessage;
    if (!backendComputeZoomPointForPixel(
            clampedPixel, nextZoom, real, imag, errorMessage)) {
        handleRenderFailure(errorMessage);
        return;
    }

    _state.zoom = nextZoom;
    _pointRealText = real;
    _pointImagText = imag;
    syncStatePointFromText();
    syncStateReadouts();
    requestRender();
}

void mandelbrotgui::zoomAtPixel(const QPoint &pixel, bool zoomIn, bool realtimeStep) {
    const QPoint clampedPixel = clampPixelToOutput(pixel);
    const double factor = currentZoomFactor(_state.zoomRate);
    const double scaleMultiplier = zoomIn ? (1.0 / factor) : factor;
    scaleAtPixel(clampedPixel, scaleMultiplier, realtimeStep);
}

void mandelbrotgui::boxZoom(const QRect &selectionRect) {
    const QRect rect = selectionRect.normalized();
    if (rect.width() < 2 || rect.height() < 2) {
        zoomAtPixel(rect.center(), true);
        return;
    }

    const QSize size = outputSize();
    const double xScale = static_cast<double>(size.width()) / rect.width();
    const double yScale = static_cast<double>(size.height()) / rect.height();
    const double factor = std::max(1.01, std::min(xScale, yScale));
    const QPoint center = clampPixelToOutput(rect.center());
    const double nextZoom = clampGuiZoom(_state.zoom + std::log10(factor));

    QString targetReal;
    QString targetImag;
    QString errorMessage;
    if (!backendPointAtPixel(center, targetReal, targetImag, errorMessage)) {
        handleRenderFailure(errorMessage);
        return;
    }

    _state.zoom = nextZoom;
    _pointRealText = targetReal;
    _pointImagText = targetImag.startsWith('-') ? targetImag.mid(1)
                                                : QString("-%1").arg(targetImag);
    syncStatePointFromText();
    syncStateReadouts();
    requestRender();
}

void mandelbrotgui::panByPixels(const QPoint &delta) {
    if (delta.isNull()) return;

    const QSize size = outputSize();
    _state.point.rx() -= static_cast<double>(delta.x()) /
        std::max(1, size.width()) * currentRealScale();
    _state.point.ry() += static_cast<double>(delta.y()) /
        std::max(1, size.height()) * currentImagScale();
    syncPointTextFromState();
    syncStateReadouts();
    requestRender();
}

void mandelbrotgui::pickAtPixel(const QPoint &pixel) {
    const QPoint clampedPixel = clampPixelToOutput(pixel);
    switch (_selectionTarget) {
        case SelectionTarget::zoomPoint:
        {
            QString real;
            QString imag;
            QString errorMessage;
            if (!backendPointAtPixel(clampedPixel, real, imag, errorMessage)) {
                handleRenderFailure(errorMessage);
                return;
            }

            _pointRealText = real;
            _pointImagText = imag;
            syncStatePointFromText();
            break;
        }
        case SelectionTarget::seedPoint:
            _state.seed = outputPixelToComplex(clampedPixel);
            break;
        case SelectionTarget::lightPoint:
            _state.light = outputPixelToComplex(clampedPixel);
            break;
    }

    _pendingPickAction = PendingPickAction{ _selectionTarget, clampedPixel };
    syncStateReadouts();
    requestRender();
}

void mandelbrotgui::updateMouseCoords(const QPoint &pixel) {
    _lastMousePixel = clampPixelToOutput(pixel);
    QString real;
    QString imag;
    QString errorMessage;
    if (backendPointAtPixel(_lastMousePixel, real, imag, errorMessage)) {
        _mouseText = QString("Mouse: %1, %2  |  %3  %4")
            .arg(_lastMousePixel.x())
            .arg(_lastMousePixel.y())
            .arg(real)
            .arg(imag);
    } else {
        _mouseText = QString("Mouse: %1, %2  |  -")
            .arg(_lastMousePixel.x())
            .arg(_lastMousePixel.y());
    }
    if (_viewport) _viewport->update();
}

void mandelbrotgui::onViewportClosed() {
    if (_closing) return;
    close();
}

void mandelbrotgui::adjustIterationsBy(int delta) {
    if (!_iterationsSpin) return;
    _iterationsSpin->setValue(std::max(0, _iterationsSpin->value() + delta));
}

void mandelbrotgui::cycleNavMode() {
    if (!_navModeCombo) return;
    _navModeCombo->setCurrentIndex((_navModeCombo->currentIndex() + 1) % 3);
}

QSize mandelbrotgui::outputSize() const {
    return { _state.outputWidth, _state.outputHeight };
}

QString mandelbrotgui::viewportStatusText() const {
    QString text;
    if (!_viewportFpsText.isEmpty()) {
        text = _viewportFpsText;
        if (!_viewportRenderTimeText.isEmpty()) {
            text += QString("    %1").arg(_viewportRenderTimeText);
        }
    }
    if (!_mouseText.isEmpty()) {
        text = text.isEmpty() ? _mouseText :
            QString("%1\n%2").arg(text, _mouseText);
    }
    return text;
}

void mandelbrotgui::closeEvent(QCloseEvent *event) {
    _closing = true;
    if (_viewport) {
        _viewport->close();
        _viewport = nullptr;
    }

    finishRenderThread();
    QWidget::closeEvent(event);
}

void mandelbrotgui::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);

    if (_controlWindowSized) return;

    updateControlWindowSize();
    _controlWindowSized = true;
}

void mandelbrotgui::updatePreview(const QImage &image) {
    _previewImage = image;
    if (_viewport) {
        static_cast<ViewportWindow *>(_viewport)->clearPreviewOffset();
    }
    updateViewport();
}

void mandelbrotgui::updateStatusLabels(const QString &rightText) {
    if (_progressLabel) {
        _progressLabel->setText(QString("%1%").arg(_progressActive ? _progressValue : 0));
    }
    if (_progressBar) {
        _progressBar->setValue(_progressActive ? _progressValue : 0);
        _progressBar->setTextVisible(false);
    }
    if (_imageMemoryLabel) {
        _imageMemoryLabel->setText(_imageMemoryText);
    }
    _statusRightLabel->setText(rightText.isEmpty() ?
        _statusText : rightText);
}

void mandelbrotgui::updateLightColorButton() {
    if (!_lightColorButton) return;

    const QColor color = lightColorToQColor(_state.lightColor);
    _lightColorButton->setText(QString::fromStdString(
        FormatUtil::formatHexColor(
            _state.lightColor.R,
            _state.lightColor.G,
            _state.lightColor.B)));
    _lightColorButton->setStyleSheet(lightColorButtonStyle(color));
}

void mandelbrotgui::updateModeEnablement() {
    const bool paletteMode = _state.colorMethod == Backend::ColorMethod::palette;
    const bool sineMode = _state.colorMethod == Backend::ColorMethod::iterations ||
        _state.colorMethod == Backend::ColorMethod::smooth_iterations;
    const bool lightMode = _state.colorMethod == Backend::ColorMethod::light;
    if (_sineGroup) {
        static_cast<CollapsibleGroupBox *>(_sineGroup)->setContentEnabled(sineMode);
    }
    if (_paletteGroup) {
        static_cast<CollapsibleGroupBox *>(_paletteGroup)->setContentEnabled(paletteMode);
    }
    if (_lightGroup) {
        static_cast<CollapsibleGroupBox *>(_lightGroup)->setContentEnabled(lightMode);
    }
    updateViewport();
}

void mandelbrotgui::updateViewport() {
    if (_viewport) {
        resizeViewportWindowToImageSize();
        _viewport->update();
    }

    updateSinePreview();
    refreshPalettePreview();
    updateStatusLabels();
}

void mandelbrotgui::updateSinePreview() {
    if (!_sinePreview) return;

    auto *preview = static_cast<SinePreviewWidget *>(_sinePreview);
    const QSize widgetSize = preview->size();
    if (!widgetSize.isValid()) return;

    if (!_previewBackend || !_previewBackend.session) {
        preview->setPreviewImage({});
        return;
    }

    const auto [rangeMin, rangeMax] = preview->range();
    const Backend::Status colorStatus =
        _previewBackend.session->setSinePalette(_state.sinePalette);
    if (!colorStatus) {
        preview->setPreviewImage({});
        return;
    }

    const int previewWidth = std::max(1, widgetSize.width() - 12);
    const int previewHeight = std::max(1, widgetSize.height() - 28);
    const QImage image = imageViewToImage(_previewBackend.session->renderSinePreview(
        previewWidth,
        previewHeight,
        static_cast<float>(rangeMin),
        static_cast<float>(rangeMax)));
    preview->setPreviewImage(image);
}

void mandelbrotgui::refreshPalettePreview() {
    if (!_palettePreviewLabel) return;

    const QSize targetSize = _palettePreviewLabel->size();
    if (!targetSize.isValid()) return;
    auto *preview = static_cast<PalettePreviewWidget *>(_palettePreviewLabel);

    if (!_palettePreviewDirty &&
        !_palettePreviewPixmap.isNull() &&
        _palettePreviewSize == targetSize) {
        preview->setPreviewPixmap(_palettePreviewPixmap);
        return;
    }

    const QImage image = makePalettePreviewImage(_state.palette, 256, 20);
    if (image.isNull()) {
        preview->clearPreview();
        return;
    }

    _palettePreviewPixmap = QPixmap::fromImage(image).scaled(
        targetSize,
        Qt::IgnoreAspectRatio,
        Qt::SmoothTransformation
    );
    _palettePreviewSize = targetSize;
    _palettePreviewDirty = false;
    preview->setPreviewPixmap(_palettePreviewPixmap);
}

void mandelbrotgui::updateControlWindowSize() {
    if (layout()) layout()->activate();
    if (_controlScrollContent && _controlScrollContent->layout()) {
        _controlScrollContent->layout()->activate();
        _controlScrollContent->updateGeometry();
    }

    QSize target = sizeHint().expandedTo(minimumSizeHint());
    const QMargins outerMargins = layout() ? layout()->contentsMargins() : QMargins();
    int controlContentHeight = 0;
    int defaultVisibleContentHeight = 0;
    if (_controlScrollContent) {
        const QSize contentSize = _controlScrollContent->sizeHint()
            .expandedTo(_controlScrollContent->minimumSizeHint());
        const int contentWidth = contentSize.width();
        controlContentHeight = contentSize.height();
        if (_viewportGroup) {
            const QMargins contentMargins = _controlScrollContent->contentsMargins();
            defaultVisibleContentHeight = std::max(
                0,
                _viewportGroup->geometry().bottom() + 1 + contentMargins.bottom());
        }
        target.setWidth(std::max(
            target.width(),
            contentWidth +
                outerMargins.left() + outerMargins.right() +
                kControlScrollBarWidth));
    } else {
        target += QSize(kControlScrollBarWidth, 0);
    }
    const int minHeight = std::max(1, minimumSizeHint().height());

    setMinimumWidth(target.width());
    setMaximumWidth(target.width());
    setMinimumHeight(minHeight);
    setMaximumHeight(QWIDGETSIZE_MAX);

    if (!_controlWindowSized) {
        int fixedPanelsHeight = 0;
        if (QLayout *outerLayout = layout()) {
            const QMargins margins = outerLayout->contentsMargins();
            fixedPanelsHeight += margins.top() + margins.bottom();

            int visibleWidgets = 0;
            for (int i = 0; i < outerLayout->count(); ++i) {
                QLayoutItem *item = outerLayout->itemAt(i);
                if (!item) continue;

                QWidget *widget = item->widget();
                if (!widget) continue;

                ++visibleWidgets;
                if (widget == _controlScrollArea) continue;
                fixedPanelsHeight += item->sizeHint().height();
            }

            fixedPanelsHeight += std::max(0, visibleWidgets - 1) * outerLayout->spacing();
        }

        const int desiredContentHeight = defaultVisibleContentHeight > 0 ?
            defaultVisibleContentHeight :
            controlContentHeight;
        int desiredHeight = std::max(minHeight, fixedPanelsHeight + desiredContentHeight);
        if (const QScreen *screen =
            (windowHandle() && windowHandle()->screen()) ?
            windowHandle()->screen() :
            (this->screen() ? this->screen() : QApplication::primaryScreen())) {
            const int maxOnScreenHeight = std::max(
                minHeight,
                screen->availableGeometry().height() - kControlWindowScreenPadding);
            desiredHeight = std::min(desiredHeight, maxOnScreenHeight);
        }

        resize(target.width(), desiredHeight);
    } else if (width() != target.width()) {
        resize(target.width(), std::max(height(), minHeight));
    }
}

void mandelbrotgui::handleRenderFailure(const QString &message) {
    if (message.isEmpty()) return;

    _statusText = message;
    if (_lastRenderFailureMessage == message) {
        return;
    }

    _lastRenderFailureMessage = message;
    QMessageBox::warning(this, "Render", message);
}

void mandelbrotgui::syncStateFromControls() {
    _state.variant = _variantCombo->currentText();
    _state.iterations = _iterationsSpin->value();
    _state.useThreads = _useThreadsCheckBox->isChecked();
    _state.julia = _juliaCheck->isChecked();
    _state.inverse = _inverseCheck->isChecked();
    _state.aaPixels = _aaSpin->value();
    _state.preserveRatio = _preserveRatioCheck->isChecked();
    _state.panRate = _panRateSlider->value();
    _state.zoomRate = _zoomRateSlider->value();
    _state.exponent = std::max(1.01, _exponentSpin->value());
    _state.sineName = _sineCombo ? _sineCombo->currentText() : _state.sineName;
    _state.sinePalette.freqR = static_cast<float>(_freqRSpin->value());
    _state.sinePalette.freqG = static_cast<float>(_freqGSpin->value());
    _state.sinePalette.freqB = static_cast<float>(_freqBSpin->value());
    _state.sinePalette.freqMult = static_cast<float>(_freqMultSpin->value());
    _state.paletteName = _paletteCombo ? _paletteCombo->currentText() : _state.paletteName;
    _state.palette.totalLength = static_cast<float>(_paletteLengthSpin->value());
    _state.palette.offset = static_cast<float>(_paletteOffsetSpin->value());
    _state.outputWidth = _outputWidthSpin->value();
    _state.outputHeight = _outputHeightSpin->value();

    switch (_colorMethodCombo->currentIndex()) {
        case 0: _state.colorMethod = Backend::ColorMethod::iterations; break;
        case 1: _state.colorMethod = Backend::ColorMethod::smooth_iterations; break;
        case 2: _state.colorMethod = Backend::ColorMethod::palette; break;
        case 3: _state.colorMethod = Backend::ColorMethod::light; break;
    }

    switch (_fractalCombo->currentIndex()) {
        case 0: _state.fractalType = Backend::FractalType::mandelbrot; break;
        case 1: _state.fractalType = Backend::FractalType::perpendicular; break;
        case 2: _state.fractalType = Backend::FractalType::burningship; break;
    }
}

void mandelbrotgui::syncControlsFromState() {
    const QSignalBlocker variantBlocker(_variantCombo);
    const QSignalBlocker iterationsBlocker(_iterationsSpin);
    const QSignalBlocker threadsBlocker(_useThreadsCheckBox);
    const QSignalBlocker juliaBlocker(_juliaCheck);
    const QSignalBlocker inverseBlocker(_inverseCheck);
    const QSignalBlocker aaBlocker(_aaSpin);
    const QSignalBlocker preserveRatioBlocker(_preserveRatioCheck);
    const QSignalBlocker panSliderBlocker(_panRateSlider);
    const QSignalBlocker panSpinBlocker(_panRateSpin);
    const QSignalBlocker zoomSliderBlocker(_zoomRateSlider);
    const QSignalBlocker zoomSpinBlocker(_zoomRateSpin);
    const QSignalBlocker exponentSliderBlocker(_exponentSlider);
    const QSignalBlocker exponentSpinBlocker(_exponentSpin);
    const QSignalBlocker sineComboBlocker(_sineCombo);
    const QSignalBlocker freqRBlocker(_freqRSpin);
    const QSignalBlocker freqGBlocker(_freqGSpin);
    const QSignalBlocker freqBBlocker(_freqBSpin);
    const QSignalBlocker freqMultBlocker(_freqMultSpin);
    const QSignalBlocker paletteComboBlocker(_paletteCombo);
    const QSignalBlocker paletteLengthBlocker(_paletteLengthSpin);
    const QSignalBlocker paletteOffsetBlocker(_paletteOffsetSpin);
    const QSignalBlocker widthBlocker(_outputWidthSpin);
    const QSignalBlocker heightBlocker(_outputHeightSpin);
    const QSignalBlocker colorBlocker(_colorMethodCombo);
    const QSignalBlocker fractalBlocker(_fractalCombo);

    _variantCombo->setCurrentText(_state.variant);
    _iterationsSpin->setValue(_state.iterations);
    _useThreadsCheckBox->setChecked(_state.useThreads);
    _juliaCheck->setChecked(_state.julia);
    _inverseCheck->setChecked(_state.inverse);
    _aaSpin->setValue(_state.aaPixels);
    _preserveRatioCheck->setChecked(_state.preserveRatio);
    _panRateSlider->setValue(_state.panRate);
    _panRateSpin->setValue(_state.panRate);
    _zoomRateSlider->setValue(_state.zoomRate);
    _zoomRateSpin->setValue(_state.zoomRate);
    _exponentSlider->setValue(
        static_cast<int>(std::round(std::max(1.01, _state.exponent) * 100.0)));
    _exponentSpin->setValue(std::max(1.01, _state.exponent));
    if (_sineCombo && !_state.sineName.isEmpty()) {
        _sineCombo->setCurrentText(_state.sineName);
    }
    _freqRSpin->setValue(_state.sinePalette.freqR);
    _freqGSpin->setValue(_state.sinePalette.freqG);
    _freqBSpin->setValue(_state.sinePalette.freqB);
    _freqMultSpin->setValue(_state.sinePalette.freqMult);
    if (_paletteCombo && !_state.paletteName.isEmpty()) {
        _paletteCombo->setCurrentText(_state.paletteName);
    }
    _paletteLengthSpin->setValue(_state.palette.totalLength);
    _paletteOffsetSpin->setValue(_state.palette.offset);
    _outputWidthSpin->setValue(_state.outputWidth);
    _outputHeightSpin->setValue(_state.outputHeight);
    _colorMethodCombo->setCurrentIndex(static_cast<int>(_state.colorMethod));
    _fractalCombo->setCurrentIndex(static_cast<int>(_state.fractalType));

    updateLightColorButton();
    syncStateReadouts();
    updateSinePreview();
}

void mandelbrotgui::syncStateReadouts() {
    if (_infoRealEdit) _infoRealEdit->setText(_pointRealText);
    if (_infoImagEdit) _infoImagEdit->setText(_pointImagText);
    if (_infoZoomEdit) _infoZoomEdit->setText(stateToString(_state.zoom, 12));
    if (_lightRealEdit) _lightRealEdit->setText(stateToString(_state.light.x(), 12));
    if (_lightImagEdit) _lightImagEdit->setText(stateToString(_state.light.y(), 12));
}

void mandelbrotgui::refreshSineList(const QString &preferredName) {
    if (!_sineCombo) return;

    QString errorMessage;
    ensureSineDirectory(errorMessage);

    QStringList names = listSineNames();
    if (names.isEmpty()) {
        names.push_back(kDefaultSineName);
    }

    const QString currentName = preferredName.isEmpty() ?
        _state.sineName :
        preferredName;
    const QString selectedName =
        names.contains(currentName, Qt::CaseInsensitive) ? currentName : names.front();

    const QSignalBlocker blocker(_sineCombo);
    _sineCombo->clear();
    _sineCombo->addItems(names);
    _sineCombo->setCurrentText(selectedName);
}

void mandelbrotgui::refreshPaletteList(const QString &preferredName) {
    if (!_paletteCombo) return;

    QString errorMessage;
    ensurePaletteDirectory(errorMessage);

    QStringList names = listPaletteNames();
    if (names.isEmpty()) {
        names.push_back(kDefaultPaletteName);
    }

    const QString currentName = preferredName.isEmpty() ?
        _state.paletteName :
        preferredName;
    const QString selectedName =
        names.contains(currentName, Qt::CaseInsensitive) ? currentName : names.front();

    const QSignalBlocker blocker(_paletteCombo);
    _paletteCombo->clear();
    _paletteCombo->addItems(names);
    _paletteCombo->setCurrentText(selectedName);
}

void mandelbrotgui::saveSine() {
    syncStateFromControls();

    QString suggested = sanitizePaletteName(_state.sineName);
    if (suggested.isEmpty()) {
        suggested = "sine";
    }

    bool accepted = false;
    const QString enteredName = QInputDialog::getText(this,
        "Save Sine Color",
        "Name",
        QLineEdit::Normal,
        suggested,
        &accepted);
    if (!accepted) return;

    const QString normalizedName = normalizePaletteName(enteredName);
    if (!isValidPaletteName(normalizedName)) {
        QMessageBox::warning(this, "Save Sine Color",
            "Use an ASCII name with letters, numbers, spaces, ., _, or -.");
        return;
    }

    QString errorMessage;
    if (!ensureSineDirectory(errorMessage)) {
        QMessageBox::warning(this, "Save Sine Color", errorMessage);
        return;
    }

    const std::filesystem::path destinationPath = sineFilePath(normalizedName);
    if (!saveSineToPath(destinationPath, _state.sinePalette, errorMessage)) {
        QMessageBox::warning(this, "Save Sine Color", errorMessage);
        return;
    }

    _state.sineName = normalizedName;
    refreshSineList(_state.sineName);
    syncControlsFromState();
}

void mandelbrotgui::savePointView() {
    syncStateFromControls();
    _state.zoom = clampGuiZoom(_state.zoom);

    QString errorMessage;
    if (!ensurePointDirectory(errorMessage)) {
        QMessageBox::warning(this, "Save View", errorMessage);
        return;
    }

    const QString defaultPath = QString::fromStdWString(
        (pointDirectoryPath() / "view.txt").wstring());
    const QString selectedPath = QFileDialog::getSaveFileName(
        this,
        "Save View",
        defaultPath,
        "View Files (*.txt);;All Files (*.*)");
    if (selectedPath.isEmpty()) return;

    PointConfig point = {
        .real = _pointRealText.toStdString(),
        .imag = _pointImagText.toStdString(),
        .zoom = _state.zoom
    };

    if (!savePointToPath(selectedPath.toStdString(), point, errorMessage)) {
        QMessageBox::warning(this, "Save View", errorMessage);
    }
}

void mandelbrotgui::loadPointView() {
    QString errorMessage;
    if (!ensurePointDirectory(errorMessage)) {
        QMessageBox::warning(this, "Load View", errorMessage);
        return;
    }

    const QString defaultPath = QString::fromStdWString(
        pointDirectoryPath().wstring());
    const QString selectedPath = QFileDialog::getOpenFileName(
        this,
        "Load View",
        defaultPath,
        "View Files (*.txt);;All Files (*.*)");
    if (selectedPath.isEmpty()) return;

    PointConfig point;
    if (!loadPointFromPath(selectedPath.toStdString(), point, errorMessage)) {
        QMessageBox::warning(this, "Load View", errorMessage);
        return;
    }

    _state.zoom = clampGuiZoom(point.zoom);
    _pointRealText = QString::fromStdString(point.real);
    _pointImagText = QString::fromStdString(point.imag);
    syncStatePointFromText();
    syncControlsFromState();
    requestRender();
}

void mandelbrotgui::importSine() {
    const QString sourcePath = QFileDialog::getOpenFileName(
        this,
        "Import Sine Color",
        QString(),
        "Sine Files (*.txt);;All Files (*.*)"
    );
    if (sourcePath.isEmpty()) return;

    Backend::SinePaletteConfig loaded;
    QString errorMessage;
    if (!loadSineFromPath(sourcePath.toStdString(), loaded, errorMessage)) {
        QMessageBox::warning(this, "Sine Color", errorMessage);
        return;
    }

    if (!ensureSineDirectory(errorMessage)) {
        QMessageBox::warning(this, "Sine Color", errorMessage);
        return;
    }

    const QStringList existingNames = listSineNames();
    const QString importedName = uniquePaletteName(
        QFileInfo(sourcePath).completeBaseName(),
        existingNames);
    const std::filesystem::path destinationPath = sineFilePath(importedName);
    if (!saveSineToPath(destinationPath, loaded, errorMessage)) {
        QMessageBox::warning(this, "Sine Color", errorMessage);
        return;
    }

    _state.sineName = importedName;
    _state.sinePalette = loaded;
    refreshSineList(_state.sineName);
    syncControlsFromState();
    requestRender();
}

bool mandelbrotgui::loadSineByName(const QString &name,
    bool requestRenderOnSuccess,
    QString *errorMessage) {
    if (errorMessage) errorMessage->clear();

    const QString normalizedName = normalizePaletteName(name);
    if (normalizedName.isEmpty()) return false;

    Backend::SinePaletteConfig loaded;
    const std::filesystem::path sourcePath = sineFilePath(normalizedName);
    QString localError;
    if (std::filesystem::exists(sourcePath)) {
        if (!loadSineFromPath(sourcePath, loaded, localError)) {
            if (errorMessage) *errorMessage = localError;
            return false;
        }
    } else if (normalizedName.compare(kDefaultSineName, Qt::CaseInsensitive) == 0) {
        loaded = defaultSinePalette();
    } else {
        localError = QString("Sine file not found: %1").arg(normalizedName);
        if (errorMessage) *errorMessage = localError;
        return false;
    }

    _state.sineName = normalizedName;
    _state.sinePalette = loaded;
    syncControlsFromState();
    updateViewport();
    if (requestRenderOnSuccess) {
        requestRender();
    }
    return true;
}

bool mandelbrotgui::loadPaletteByName(const QString &name,
    bool requestRenderOnSuccess,
    QString *errorMessage) {
    if (errorMessage) errorMessage->clear();

    const QString normalizedName = normalizePaletteName(name);
    if (normalizedName.isEmpty()) return false;

    Backend::PaletteHexConfig loaded;
    const std::filesystem::path sourcePath = paletteFilePath(normalizedName);
    QString localError;
    if (std::filesystem::exists(sourcePath)) {
        if (!loadPaletteFromPath(sourcePath, loaded, localError)) {
            if (errorMessage) *errorMessage = localError;
            return false;
        }
    } else if (normalizedName.compare(kDefaultPaletteName, Qt::CaseInsensitive) == 0) {
        loaded = defaultPalette();
    } else {
        localError = QString("Palette not found: %1").arg(normalizedName);
        if (errorMessage) *errorMessage = localError;
        return false;
    }

    _state.paletteName = normalizedName;
    _state.palette = loaded;
    _palettePreviewDirty = true;
    syncControlsFromState();
    updateViewport();
    if (requestRenderOnSuccess) {
        requestRender();
    }
    return true;
}

void mandelbrotgui::openPaletteEditor() {
    const Backend::PaletteHexConfig original = _state.palette;
    const QString originalName = _state.paletteName;
    PaletteEditorDialog dialog(_state.palette,
        _state.paletteName,
        this);

    if (dialog.exec() == QDialog::Accepted) {
        _state.palette = dialog.palette();
        _state.palette.totalLength = static_cast<float>(_paletteLengthSpin->value());
        _state.palette.offset = static_cast<float>(_paletteOffsetSpin->value());
        if (!dialog.savedPaletteName().isEmpty()) {
            _state.paletteName = dialog.savedPaletteName();
            refreshPaletteList(_state.paletteName);
        }
        _palettePreviewDirty = true;
        updateViewport();
        requestRender();
    } else {
        _state.palette = original;
        _state.paletteName = originalName;
        refreshPaletteList(_state.paletteName);
        _palettePreviewDirty = true;
        updateViewport();
    }
}

void mandelbrotgui::saveImage() {
    QString error;
    if (!ensureBackendReady(error)) {
        QMessageBox::warning(this, "Save", error);
        return;
    }

    if (_previewImage.isNull()) {
        QMessageBox::warning(this, "Save", "No image is available.");
        return;
    }

    auto result = runSaveDialog(this, "mandelbrot.png");
    if (!result) return;

    const Backend::Status status = _backend.session->saveImage(
        result->path.toStdString(),
        result->appendDate,
        result->type.toStdString());
    if (!status) {
        QMessageBox::warning(this, "Save",
            QString::fromStdString(status.message));
    }
}

void mandelbrotgui::resizeViewportImage() {
    syncStateFromControls();
    resizeViewportWindowToImageSize();
    requestRender(true);
}

void mandelbrotgui::updateSelectionTarget(int index) {
    _selectionTarget = static_cast<SelectionTarget>(index);
}

void mandelbrotgui::updateNavMode(int index) {
    _navMode = static_cast<NavMode>(index);
}

void mandelbrotgui::updateAspectLinkedSizes(bool widthChanged) {
    if (!_preserveRatioCheck || !_preserveRatioCheck->isChecked()) return;
    if (!_outputWidthSpin || !_outputHeightSpin) return;
    if (_state.outputWidth <= 0 || _state.outputHeight <= 0) return;

    if (widthChanged) {
        const int newWidth = _outputWidthSpin->value();
        const int newHeight = std::max(1,
            static_cast<int>(std::lround(
                static_cast<double>(newWidth) * _state.outputHeight /
                std::max(1, _state.outputWidth))));
        const QSignalBlocker blocker(_outputHeightSpin);
        _outputHeightSpin->setValue(newHeight);
        return;
    }

    const int newHeight = _outputHeightSpin->value();
    const int newWidth = std::max(1,
        static_cast<int>(std::lround(
            static_cast<double>(newHeight) * _state.outputWidth /
            std::max(1, _state.outputHeight))));
    const QSignalBlocker blocker(_outputWidthSpin);
    _outputWidthSpin->setValue(newWidth);
}

QString mandelbrotgui::stateToString(double value, int precision) const {
    return QString::number(value, 'g', precision);
}

void mandelbrotgui::syncPointTextFromState() {
    _pointRealText = stateToString(_state.point.x());
    _pointImagText = stateToString(_state.point.y());
}

void mandelbrotgui::syncStatePointFromText() {
    bool okReal = false;
    bool okImag = false;
    const double real = _pointRealText.toDouble(&okReal);
    const double imag = _pointImagText.toDouble(&okImag);
    if (okReal) _state.point.setX(real);
    if (okImag) _state.point.setY(imag);
}

bool mandelbrotgui::syncPreviewSessionForCoords(const UiState &state,
    QString &errorMessage) {
    Backend::Session *session = nullptr;
    if (_backend && _backend.session) {
        session = _backend.session.get();
    } else if (_previewBackend && _previewBackend.session) {
        session = _previewBackend.session.get();
    }
    if (!session) {
        errorMessage = "Backend not loaded.";
        return false;
    }

    auto failIfNeeded = [&errorMessage](const Backend::Status &status) {
        if (status) return false;
        errorMessage = QString::fromStdString(status.message);
        return true;
    };

    if (failIfNeeded(session->setImageSize(
            state.outputWidth, state.outputHeight, state.aaPixels)))
        return false;
    if (failIfNeeded(session->setZoom(
            state.iterations, static_cast<float>(clampGuiZoom(state.zoom)))))
        return false;
    if (failIfNeeded(session->setPoint(
            _pointRealText.toStdString(), _pointImagText.toStdString())))
        return false;

    return true;
}

bool mandelbrotgui::backendPointAtPixel(const QPoint &pixel, QString &real,
    QString &imag, QString &errorMessage) {
    if (!syncPreviewSessionForCoords(_state, errorMessage)) return false;

    Backend::Session *session = nullptr;
    if (_backend && _backend.session) {
        session = _backend.session.get();
    } else if (_previewBackend && _previewBackend.session) {
        session = _previewBackend.session.get();
    }
    if (!session) {
        errorMessage = "Backend not loaded.";
        return false;
    }

    std::string realOut;
    std::string imagOut;
    const Backend::Status status = session->getPointAtPixel(
        pixel.x(), pixel.y(), realOut, imagOut);
    if (!status) {
        errorMessage = QString::fromStdString(status.message);
        return false;
    }

    real = QString::fromStdString(realOut);
    imag = QString::fromStdString(imagOut);
    return true;
}

bool mandelbrotgui::backendComputeZoomPointForPixel(const QPoint &pixel,
    double targetZoom, QString &real, QString &imag, QString &errorMessage) {
    if (!syncPreviewSessionForCoords(_state, errorMessage)) return false;

    Backend::Session *session = nullptr;
    if (_backend && _backend.session) {
        session = _backend.session.get();
    } else if (_previewBackend && _previewBackend.session) {
        session = _previewBackend.session.get();
    }
    if (!session) {
        errorMessage = "Backend not loaded.";
        return false;
    }

    std::string realOut;
    std::string imagOut;
    const Backend::Status status = session->computeZoomPointForPixel(
        pixel.x(), pixel.y(), static_cast<float>(targetZoom), realOut, imagOut);
    if (!status) {
        errorMessage = QString::fromStdString(status.message);
        return false;
    }

    real = QString::fromStdString(realOut);
    imag = QString::fromStdString(imagOut);
    return true;
}

QPoint mandelbrotgui::clampPixelToOutput(const QPoint &pixel) const {
    const QSize size = outputSize();
    return {
        std::clamp(pixel.x(), 0, std::max(0, size.width() - 1)),
        std::clamp(pixel.y(), 0, std::max(0, size.height() - 1))
    };
}

QPointF mandelbrotgui::outputPixelToComplex(const QPoint &pixel) const {
    const QPoint clampedPixel = clampPixelToOutput(pixel);
    const QSize size = outputSize();
    const double dx = (clampedPixel.x() - size.width() / 2.0) /
        std::max(1, size.width());
    const double dy = (clampedPixel.y() - size.height() / 2.0) /
        std::max(1, size.height());

    return {
        dx * currentRealScale() + _state.point.x(),
        dy * currentImagScale() - _state.point.y()
    };
}

double mandelbrotgui::currentRealScale() const {
    return 1.0 / std::pow(10.0, _state.zoom);
}

double mandelbrotgui::currentImagScale() const {
    const QSize size = outputSize();
    const double aspect = static_cast<double>(size.width()) /
        std::max(1, size.height());
    return currentRealScale() / aspect;
}

void mandelbrotgui::resizeViewportWindowToImageSize() {
    resizeViewportToImageSize(_viewport, outputSize());
}

double mandelbrotgui::currentZoomFactor(int sliderValue) const {
    return kZoomStepTable[clampSliderValue(sliderValue)];
}
