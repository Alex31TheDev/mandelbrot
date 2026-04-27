#pragma once

#include <filesystem>

#include <QString>

#include "parsers/point/PointParser.h"

namespace GUI::PointStore {
[[nodiscard]] std::filesystem::path directoryPath();
[[nodiscard]] bool ensureDirectory(QString& errorMessage);
[[nodiscard]] bool loadFromPath(const std::filesystem::path& path,
    PointConfig& point, QString& errorMessage);
[[nodiscard]] bool saveToPath(const std::filesystem::path& path,
    const PointConfig& point, QString& errorMessage);
}
