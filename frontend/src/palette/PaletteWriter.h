#pragma once

#include <string>
#include <variant>

#include "BackendAPI.h"

class PaletteWriter {
public:
    explicit PaletteWriter(const Backend::PaletteRGBConfig &palette);
    explicit PaletteWriter(const Backend::PaletteHexConfig &palette);

    bool write(const std::string &filePath, std::string &err) const;

private:
    std::variant<Backend::PaletteRGBConfig, Backend::PaletteHexConfig> _palette;
};
