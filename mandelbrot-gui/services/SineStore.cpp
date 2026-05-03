#include "SineStore.h"

#include <algorithm>
#include <system_error>

#include <QCoreApplication>

#include "BackendAPI.h"
using namespace Backend;

#include "parsers/sine/SineParser.h"
#include "parsers/sine/SineWriter.h"

#include "util/FileUtil.h"
#include "util/NumberUtil.h"

namespace GUI::SineStore {
    SinePaletteConfig makeNewConfig() {
        return { .freqR = 1.0f, .freqG = 1.0f, .freqB = 1.0f, .freqMult = 1.0f };
    }

    bool sameConfig(
        const SinePaletteConfig &a,
        const SinePaletteConfig &b
    ) {
        return NumberUtil::almostEqual(a.freqR, b.freqR)
            && NumberUtil::almostEqual(a.freqG, b.freqG)
            && NumberUtil::almostEqual(a.freqB, b.freqB)
            && NumberUtil::almostEqual(a.freqMult, b.freqMult);
    }

    std::filesystem::path directoryPath() {
        return FileUtil::executableDir() / "sines";
    }

    std::filesystem::path filePath(const QString &name) {
        return directoryPath()
            / std::filesystem::path((name + ".txt").toStdString());
    }

    QStringList listNames() {
        QStringList names;
        std::error_code ec;
        const std::filesystem::path dir = directoryPath();
        if (!std::filesystem::exists(dir, ec) || ec) {
            return names;
        }

        for (const auto &entry : std::filesystem::directory_iterator(dir, ec)) {
            if (ec) break;
            if (!entry.is_regular_file()) continue;

            const std::filesystem::path path = entry.path();
            if (path.extension() != ".txt") continue;
            const QString stem = QString::fromStdWString(path.stem().wstring());
            if (stem.startsWith('.') && stem.endsWith("_recovery")) continue;
            names.push_back(QString::fromStdWString(path.stem().wstring()));
        }

        names.removeDuplicates();
        std::sort(
            names.begin(), names.end(), [](const QString &a, const QString &b) {
                if (a.compare(defaultName, Qt::CaseInsensitive) == 0) return true;
                if (b.compare(defaultName, Qt::CaseInsensitive) == 0) return false;
                return a.compare(b, Qt::CaseInsensitive) < 0;
            }
        );
        return names;
    }

    bool ensureDirectory(QString &errorMessage) {
        errorMessage.clear();

        std::error_code ec;
        std::filesystem::create_directories(directoryPath(), ec);
        if (!ec) return true;

        errorMessage = QCoreApplication::translate("SineStore",
            "Failed to create sine directory: %1")
            .arg(QString::fromStdString(ec.message()));
        return false;
    }

    bool loadFromPath(
        const std::filesystem::path &path,
        SinePaletteConfig &palette, QString &errorMessage
    ) {
        SineParser parser("-");
        std::string err;
        if (parser.parse({ path.string() }, palette, err)) {
            errorMessage.clear();
            return true;
        }

        errorMessage = QString::fromStdString(err);
        return false;
    }

    bool saveToPath(
        const std::filesystem::path &path,
        const SinePaletteConfig &palette, QString &errorMessage
    ) {
        SineWriter writer(palette);
        std::string err;
        if (writer.write(path.string(), err)) {
            errorMessage.clear();
            return true;
        }

        errorMessage = QString::fromStdString(err);
        return false;
    }
}