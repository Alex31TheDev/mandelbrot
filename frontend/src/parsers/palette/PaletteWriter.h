#pragma once
#include "../KeyValueWriter.h"

#include <iosfwd>
#include <string>
#include <variant>

#include "BackendAPI.h"

class PaletteWriter : public KeyValueWriter {
public:
    explicit PaletteWriter(const Backend::PaletteRGBConfig &palette);
    explicit PaletteWriter(const Backend::PaletteHexConfig &palette);

    bool write(const std::string &filePath, std::string &err) const;

private:
    std::variant<
        Backend::PaletteRGBConfig,
        Backend::PaletteHexConfig
    > _palette;

    void _writeBody(std::ostream &out) const override;

    std::string _headerLine() const override {
        return "Palette file";
    }
    std::string _openFileError() const override {
        return "Failed to open palette file for writing.";
    }
    std::string _writeFileError() const override {
        return "Failed while writing palette file.";
    }
};
