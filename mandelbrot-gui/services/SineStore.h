#pragma once

#include <filesystem>

#include <QString>
#include <QStringList>

#include "BackendAPI.h"

namespace GUI::SineStore {
    using namespace Backend;

    inline constexpr auto defaultName = "default";

    [[nodiscard]] SinePaletteConfig makeNewConfig();
    [[nodiscard]] bool sameConfig(
        const SinePaletteConfig &a, const SinePaletteConfig &b);

    [[nodiscard]] std::filesystem::path directoryPath();
    [[nodiscard]] std::filesystem::path filePath(const QString &name);
    [[nodiscard]] QStringList listNames();
    [[nodiscard]] bool ensureDirectory(QString &errorMessage);
    [[nodiscard]] bool loadFromPath(const std::filesystem::path &path,
        SinePaletteConfig &palette, QString &errorMessage);
    [[nodiscard]] bool saveToPath(const std::filesystem::path &path,
        const SinePaletteConfig &palette, QString &errorMessage);
}
