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
inline constexpr auto kDefaultName = "default";
inline constexpr auto kNewEntryLabel = "+ New";
inline constexpr auto kUnsavedLabelSuffix = " (unsaved)";

[[nodiscard]] Backend::PaletteHexConfig makeNewConfig();
[[nodiscard]] bool sameConfig(
    const Backend::PaletteHexConfig& a, const Backend::PaletteHexConfig& b);

[[nodiscard]] std::filesystem::path directoryPath();
[[nodiscard]] std::filesystem::path filePath(const QString& name);
[[nodiscard]] bool ensureDirectory(QString& errorMessage);

[[nodiscard]] QString normalizeName(const QString& name);
[[nodiscard]] bool isValidName(const QString& name);
[[nodiscard]] QString sanitizeName(const QString& name);
[[nodiscard]] QStringList listNames();
[[nodiscard]] QString uniqueName(
    const QString& baseName, const QStringList& existingNames);

[[nodiscard]] bool loadFromPath(const std::filesystem::path& path,
    Backend::PaletteHexConfig& palette, QString& errorMessage);
[[nodiscard]] bool saveToPath(const std::filesystem::path& path,
    const Backend::PaletteHexConfig& palette, QString& errorMessage);
[[nodiscard]] bool loadNamed(const QString& name,
    Backend::PaletteHexConfig& palette, QString& errorMessage);
[[nodiscard]] bool importFromPath(const std::filesystem::path& sourcePath,
    float totalLength, float offset, QString& importedName,
    Backend::PaletteHexConfig& palette, std::filesystem::path& destinationPath,
    QString& errorMessage);
[[nodiscard]] bool saveNamed(const QString& name,
    const Backend::PaletteHexConfig& palette,
    std::filesystem::path& destinationPath, QString& errorMessage);
[[nodiscard]] bool saveFromDialogPath(const QString& savePath,
    const Backend::PaletteHexConfig& palette, QString& savedName,
    std::filesystem::path& destinationPath, QString& errorMessage);

[[nodiscard]] std::vector<PaletteStop> configToStops(
    const Backend::PaletteHexConfig& palette);
[[nodiscard]] Backend::PaletteHexConfig stopsToConfig(
    const std::vector<PaletteStop>& stops, float totalLength, float offset,
    bool blendEnds);

[[nodiscard]] QImage makePreviewImage(Backend::Session* session,
    const Backend::PaletteHexConfig& palette, int width, int height);
}
