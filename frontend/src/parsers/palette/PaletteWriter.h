#pragma once

#include <iosfwd>
#include <string>
#include <variant>

#include "BackendAPI.h"
#include "../KeyValueWriter.h"

class PaletteWriter : public KeyValueWriter<PaletteWriter> {
public:
    explicit PaletteWriter(const Backend::PaletteRGBConfig &palette);
    explicit PaletteWriter(const Backend::PaletteHexConfig &palette);

    bool write(const std::string &filePath, std::string &err) const;

private:
    friend class KeyValueWriter<PaletteWriter>;

    inline static const std::string _headerLine = "Palette file";
    inline static const std::string _openFileError =
        "Failed to open palette file for writing.";
    inline static const std::string _writeFileError =
        "Failed while writing palette file.";

    void _writeBody(std::ostream &out) const override;

    std::variant<Backend::PaletteRGBConfig, Backend::PaletteHexConfig> _palette;
};
