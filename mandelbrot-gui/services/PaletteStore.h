#pragma once

#include <filesystem>
#include <optional>
#include <vector>

#include <QImage>
#include <QString>
#include <QStringList>

#include "BackendAPI.h"
#include "widgets/PaletteTypes.h"

namespace GUI::PaletteStore {
    using namespace Backend;

    inline constexpr auto defaultName = "default";

    [[nodiscard]] PaletteRGBConfig makeNewConfig();
    [[nodiscard]] bool sameConfig(const PaletteRGBConfig &a,
        const PaletteRGBConfig &b);

    [[nodiscard]] std::filesystem::path directoryPath();
    [[nodiscard]] std::filesystem::path filePath(const QString &name);
    [[nodiscard]] bool ensureDirectory(QString &errorMessage);

    [[nodiscard]] QString normalizeName(const QString &name);
    [[nodiscard]] bool isValidName(const QString &name);
    [[nodiscard]] QString sanitizeName(const QString &name);
    [[nodiscard]] QStringList listNames();
    [[nodiscard]] QString uniqueName(const QString &baseName,
        const QStringList &existingNames);

    [[nodiscard]] bool loadFromPath(const std::filesystem::path &path,
        PaletteRGBConfig &palette, QString &errorMessage);
    [[nodiscard]] bool saveToPath(const std::filesystem::path &path,
        const PaletteRGBConfig &palette, QString &errorMessage);
    [[nodiscard]] bool loadNamed(const QString &name,
        PaletteRGBConfig &palette, QString &errorMessage);
    [[nodiscard]] bool importFromPath(const std::filesystem::path &sourcePath,
        float totalLength, float offset, QString &importedName,
        PaletteRGBConfig &palette, std::filesystem::path &destinationPath,
        QString &errorMessage);
    [[nodiscard]] bool saveNamed(const QString &name,
        const PaletteRGBConfig &palette,
        std::filesystem::path &destinationPath, QString &errorMessage);
    [[nodiscard]] bool saveFromDialogPath(const QString &savePath,
        const PaletteRGBConfig &palette, QString &savedName,
        std::filesystem::path &destinationPath, QString &errorMessage);

    [[nodiscard]] std::vector<PaletteStop> configToStops(
        const PaletteRGBConfig &palette
    );
    [[nodiscard]] PaletteRGBConfig stopsToConfig(
        const std::vector<PaletteStop> &stops,
        float totalLength, float offset, bool blendEnds
    );

    [[nodiscard]] QImage makePreviewImage(Session *session,
        const PaletteRGBConfig &palette, int width, int height);
}
