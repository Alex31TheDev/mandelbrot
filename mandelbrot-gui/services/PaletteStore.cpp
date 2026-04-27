#include "services/PaletteStore.h"

#include <algorithm>
#include <cmath>
#include <system_error>

#include <QColor>

#include "parsers/palette/PaletteParser.h"
#include "parsers/palette/PaletteWriter.h"
#include "util/FormatUtil.h"
#include "util/NumberUtil.h"
#include "util/PathUtil.h"

Backend::PaletteHexConfig GUI::PaletteStore::makeNewConfig() {
    Backend::PaletteHexConfig palette;
    palette.totalLength = 1.0f;
    palette.offset = 0.0f;
    palette.blendEnds = false;
    palette.entries = { { "#000000", 1.0f }, { "#FFFFFF", 0.0f } };
    return palette;
}

bool GUI::PaletteStore::sameConfig(
    const Backend::PaletteHexConfig& a, const Backend::PaletteHexConfig& b) {
    if (!NumberUtil::almostEqual(a.totalLength, b.totalLength)
        || !NumberUtil::almostEqual(a.offset, b.offset)
        || a.blendEnds != b.blendEnds || a.entries.size() != b.entries.size()) {
        return false;
    }

    for (size_t i = 0; i < a.entries.size(); ++i) {
        if (!NumberUtil::almostEqual(a.entries[i].length, b.entries[i].length)
            || QString::fromStdString(a.entries[i].color)
                    .compare(QString::fromStdString(b.entries[i].color),
                        Qt::CaseInsensitive)
                != 0) {
            return false;
        }
    }

    return true;
}

std::filesystem::path GUI::PaletteStore::directoryPath() {
    return PathUtil::executableDir() / "palettes";
}

std::filesystem::path GUI::PaletteStore::filePath(const QString& name) {
    return directoryPath()
        / std::filesystem::path((name + ".txt").toStdString());
}

bool GUI::PaletteStore::ensureDirectory(QString& errorMessage) {
    errorMessage.clear();

    std::error_code ec;
    std::filesystem::create_directories(directoryPath(), ec);
    if (!ec) return true;

    errorMessage = QString("Failed to create palette directory: %1")
                       .arg(QString::fromStdString(ec.message()));
    return false;
}

QString GUI::PaletteStore::normalizeName(const QString& name) {
    return name.trimmed();
}

bool GUI::PaletteStore::isValidName(const QString& name) {
    const QString trimmed = normalizeName(name);
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

QString GUI::PaletteStore::sanitizeName(const QString& name) {
    QString sanitized = normalizeName(name);
    for (QChar& ch : sanitized) {
        const bool allowed = ch.unicode() <= 0x7F
            && (ch.isLetterOrNumber() || ch == ' ' || ch == '_' || ch == '-'
                || ch == '.');
        if (!allowed) ch = '_';
    }

    sanitized = sanitized.simplified();
    if (sanitized.isEmpty() || sanitized == "." || sanitized == "..") {
        sanitized = "palette";
    }

    return sanitized;
}

QStringList GUI::PaletteStore::listNames() {
    QStringList names;
    std::error_code ec;
    const std::filesystem::path dir = directoryPath();
    if (!std::filesystem::exists(dir, ec) || ec) {
        return names;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;

        const std::filesystem::path path = entry.path();
        if (path.extension() != ".txt") continue;
        names.push_back(QString::fromStdWString(path.stem().wstring()));
    }

    names.removeDuplicates();
    std::sort(
        names.begin(), names.end(), [](const QString& a, const QString& b) {
            if (a.compare(kDefaultName, Qt::CaseInsensitive) == 0) return true;
            if (b.compare(kDefaultName, Qt::CaseInsensitive) == 0) return false;
            return a.compare(b, Qt::CaseInsensitive) < 0;
        });
    return names;
}

QString GUI::PaletteStore::uniqueName(
    const QString& baseName, const QStringList& existingNames) {
    const QString sanitized = sanitizeName(baseName);
    if (!existingNames.contains(sanitized, Qt::CaseInsensitive)) {
        return sanitized;
    }

    for (int suffix = 2;; ++suffix) {
        const QString candidate = QString("%1 %2").arg(sanitized).arg(suffix);
        if (!existingNames.contains(candidate, Qt::CaseInsensitive)) {
            return candidate;
        }
    }
}

bool GUI::PaletteStore::loadFromPath(const std::filesystem::path& path,
    Backend::PaletteHexConfig& palette, QString& errorMessage) {
    PaletteParser parser("-");
    std::string err;
    if (parser.parse({ path.string() }, palette, err)) {
        errorMessage.clear();
        return true;
    }

    errorMessage = QString::fromStdString(err);
    return false;
}

bool GUI::PaletteStore::saveToPath(const std::filesystem::path& path,
    const Backend::PaletteHexConfig& palette, QString& errorMessage) {
    PaletteWriter writer(palette);
    std::string err;
    if (writer.write(path.string(), err)) {
        errorMessage.clear();
        return true;
    }

    errorMessage = QString::fromStdString(err);
    return false;
}

static double paletteSegmentLengthSum(
    const Backend::PaletteHexConfig& palette) {
    if (palette.entries.empty()) return 0.0;

    const size_t segmentCount = palette.blendEnds ? palette.entries.size()
                                                  : palette.entries.size() - 1;

    double total = 0.0;
    for (size_t i = 0; i < segmentCount; ++i) {
        total += std::max(0.0f, palette.entries[i].length);
    }

    return total;
}

static QColor lerpPreviewColor(const QColor& from, const QColor& to, double t) {
    const double clamped = std::clamp(t, 0.0, 1.0);
    return QColor(static_cast<int>(std::lround(
                      from.red() + (to.red() - from.red()) * clamped)),
        static_cast<int>(
            std::lround(from.green() + (to.green() - from.green()) * clamped)),
        static_cast<int>(
            std::lround(from.blue() + (to.blue() - from.blue()) * clamped)));
}

static QColor samplePreviewColor(
    const Backend::PaletteHexConfig& palette, double x) {
    if (palette.entries.empty()) return Qt::black;

    const size_t colorCount = palette.entries.size();
    size_t segmentCount = colorCount;
    if (!palette.blendEnds) {
        if (colorCount < 2) {
            return QColor(
                QString::fromStdString(palette.entries.front().color));
        }
        segmentCount--;
    }

    if (segmentCount == 0) {
        return QColor(QString::fromStdString(palette.entries.front().color));
    }

    std::vector<QColor> colors;
    colors.reserve(colorCount);
    for (const auto& entry : palette.entries) {
        QColor color(QString::fromStdString(entry.color));
        colors.push_back(color.isValid() ? color : QColor(Qt::black));
    }

    std::vector<double> accum(segmentCount + 1, 0.0);
    std::vector<double> spans(segmentCount, 0.0);
    const double totalLength
        = std::max(1.0, static_cast<double>(palette.totalLength));
    const double lengthSum = paletteSegmentLengthSum(palette);
    const double defaultSpan = totalLength / static_cast<double>(segmentCount);

    for (size_t i = 0; i < segmentCount; ++i) {
        const double rawLength
            = std::max(0.0, static_cast<double>(palette.entries[i].length));
        spans[i] = lengthSum > 0.0 ? rawLength / lengthSum * totalLength
                                   : defaultSpan;
        accum[i + 1] = accum[i] + spans[i];
    }

    const double wrappedTotal
        = palette.blendEnds ? totalLength : std::max(accum.back(), 1.0e-12);
    const double offset
        = std::clamp(static_cast<double>(palette.offset), 0.0, wrappedTotal);

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

QImage GUI::PaletteStore::makePreviewImage(
    const Backend::PaletteHexConfig& palette, int width, int height) {
    if (width <= 0 || height <= 0) return {};

    QImage image(width, height, QImage::Format_RGB32);
    const double totalLength
        = std::max(1.0, static_cast<double>(palette.totalLength));
    const double denom = width > 1 ? static_cast<double>(width - 1) : 1.0;

    for (int x = 0; x < width; ++x) {
        const QColor color = samplePreviewColor(
            palette, totalLength * static_cast<double>(x) / denom);
        const QRgb pixel = color.rgb();

        for (int y = 0; y < height; ++y) {
            image.setPixel(x, y, pixel);
        }
    }

    return image;
}

std::vector<PaletteStop> GUI::PaletteStore::configToStops(
    const Backend::PaletteHexConfig& palette) {
    std::vector<PaletteStop> stops;
    if (palette.entries.empty()) return stops;

    double accum = 0.0;
    const double segmentSum = paletteSegmentLengthSum(palette);
    const bool useEqualSpacing = segmentSum <= 0.0;
    const size_t stopCount = palette.entries.size();
    int nextId = 1;

    if (!palette.blendEnds && palette.entries.size() >= 2) {
        for (size_t i = 0; i + 1 < palette.entries.size(); ++i) {
            const auto& entry = palette.entries[i];
            stops.push_back({ nextId++,
                useEqualSpacing ? static_cast<double>(i)
                        / std::max<size_t>(1, palette.entries.size() - 1)
                                : std::clamp(accum / segmentSum, 0.0, 1.0),
                QColor(QString::fromStdString(entry.color)) });
            accum += std::max(0.0f, entry.length);
        }

        stops.push_back({ nextId++, 1.0,
            QColor(QString::fromStdString(palette.entries.back().color)) });
        return stops;
    }

    for (const auto& entry : palette.entries) {
        stops.push_back({ nextId++,
            useEqualSpacing ? static_cast<double>(stops.size())
                    / std::max<size_t>(1, stopCount)
                            : std::clamp(accum / segmentSum, 0.0, 1.0),
            QColor(QString::fromStdString(entry.color)) });
        accum += std::max(0.0f, entry.length);
    }

    if (stops.size() == 1) {
        stops.push_back({ nextId++, 1.0, stops.front().color });
    }

    return stops;
}

Backend::PaletteHexConfig GUI::PaletteStore::stopsToConfig(
    const std::vector<PaletteStop>& stops, float totalLength, float offset,
    bool blendEnds) {
    constexpr double kLoopEndpointEpsilon = 1e-4;
    Backend::PaletteHexConfig palette;
    palette.totalLength = totalLength;
    palette.offset = offset;
    palette.blendEnds = blendEnds;

    if (stops.size() < 2) {
        const auto fallback = makeNewConfig();
        palette.entries = fallback.entries;
        return palette;
    }

    std::vector<PaletteStop> sorted = stops;
    std::sort(sorted.begin(), sorted.end(),
        [](const PaletteStop& a, const PaletteStop& b) {
            return a.pos < b.pos;
        });

    if (blendEnds && sorted.size() >= 2 && sorted.back().pos >= 1.0) {
        const double minPos = std::min(1.0 - kLoopEndpointEpsilon,
            sorted[sorted.size() - 2].pos + kLoopEndpointEpsilon);
        sorted.back().pos = minPos;
    }

    if (!blendEnds) {
        for (size_t i = 0; i + 1 < sorted.size(); ++i) {
            const double start = std::clamp(sorted[i].pos, 0.0, 1.0);
            const double end = std::clamp(sorted[i + 1].pos, 0.0, 1.0);
            const double len = std::max((end - start) * totalLength, 0.0001);

            palette.entries.push_back(
                { sorted[i].color.name(QColor::HexRgb).toStdString(),
                    static_cast<float>(len) });
        }

        palette.entries.push_back(
            { sorted.back().color.name(QColor::HexRgb).toStdString(), 0.0f });
        return palette;
    }

    for (size_t i = 0; i < sorted.size(); ++i) {
        const size_t next = (i + 1) % sorted.size();
        double len = sorted[next].pos - sorted[i].pos;
        if (next == 0) len += 1.0;
        len = std::max(len * totalLength, 0.0001);

        palette.entries.push_back(
            { sorted[i].color.name(QColor::HexRgb).toStdString(),
                static_cast<float>(len) });
    }

    return palette;
}
