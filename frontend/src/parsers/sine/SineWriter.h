#pragma once

#include <string>
#include <ostream>

#include "BackendAPI.h"

#include "../KeyValueWriter.h"

class SineWriter : public KeyValueWriter {
public:
    explicit SineWriter(const Backend::SinePaletteConfig &palette);

    bool write(const std::string &filePath, std::string &err) const;

private:
    Backend::SinePaletteConfig _palette;

    void _writeBody(std::ostream &out) const override;

    std::string _headerLine() const override {
        return "Sine file";
    }
    std::string _openFileError() const override {
        return "Failed to open sine file for writing.";
    }
    std::string _writeFileError() const override {
        return "Failed while writing sine file.";
    }
};
