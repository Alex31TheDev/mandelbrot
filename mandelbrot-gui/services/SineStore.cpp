#include "services/SineStore.h"

#include <algorithm>
#include <system_error>

#include "parsers/sine/SineParser.h"
#include "parsers/sine/SineWriter.h"
#include "util/NumberUtil.h"
#include "util/PathUtil.h"

Backend::SinePaletteConfig GUI::SineStore::makeNewConfig() {
    return { .freqR = 1.0f, .freqG = 1.0f, .freqB = 1.0f, .freqMult = 1.0f };
}

bool GUI::SineStore::sameConfig(
    const Backend::SinePaletteConfig& a, const Backend::SinePaletteConfig& b) {
    return NumberUtil::almostEqual(a.freqR, b.freqR)
        && NumberUtil::almostEqual(a.freqG, b.freqG)
        && NumberUtil::almostEqual(a.freqB, b.freqB)
        && NumberUtil::almostEqual(a.freqMult, b.freqMult);
}

std::filesystem::path GUI::SineStore::directoryPath() {
    return PathUtil::executableDir() / "sines";
}

std::filesystem::path GUI::SineStore::filePath(const QString& name) {
    return directoryPath()
        / std::filesystem::path((name + ".txt").toStdString());
}

QStringList GUI::SineStore::listNames() {
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

bool GUI::SineStore::ensureDirectory(QString& errorMessage) {
    errorMessage.clear();

    std::error_code ec;
    std::filesystem::create_directories(directoryPath(), ec);
    if (!ec) return true;

    errorMessage = QString("Failed to create sine directory: %1")
                       .arg(QString::fromStdString(ec.message()));
    return false;
}

bool GUI::SineStore::loadFromPath(const std::filesystem::path& path,
    Backend::SinePaletteConfig& palette, QString& errorMessage) {
    SineParser parser("-");
    std::string err;
    if (parser.parse({ path.string() }, palette, err)) {
        errorMessage.clear();
        return true;
    }

    errorMessage = QString::fromStdString(err);
    return false;
}

bool GUI::SineStore::saveToPath(const std::filesystem::path& path,
    const Backend::SinePaletteConfig& palette, QString& errorMessage) {
    SineWriter writer(palette);
    std::string err;
    if (writer.write(path.string(), err)) {
        errorMessage.clear();
        return true;
    }

    errorMessage = QString::fromStdString(err);
    return false;
}
