#pragma once

#include <filesystem>

#include <QString>
#include <QStringList>

#include "BackendAPI.h"

namespace GUI::SineStore {
inline constexpr auto defaultName = "default";

[[nodiscard]] Backend::SinePaletteConfig makeNewConfig();
[[nodiscard]] bool sameConfig(
    const Backend::SinePaletteConfig& a, const Backend::SinePaletteConfig& b);

[[nodiscard]] std::filesystem::path directoryPath();
[[nodiscard]] std::filesystem::path filePath(const QString& name);
[[nodiscard]] QStringList listNames();
[[nodiscard]] bool ensureDirectory(QString& errorMessage);
[[nodiscard]] bool loadFromPath(const std::filesystem::path& path,
    Backend::SinePaletteConfig& palette, QString& errorMessage);
[[nodiscard]] bool saveToPath(const std::filesystem::path& path,
    const Backend::SinePaletteConfig& palette, QString& errorMessage);
}
