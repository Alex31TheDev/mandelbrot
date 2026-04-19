#pragma once

#include <iosfwd>
#include <string>

#include "BackendAPI.h"
#include "../KeyValueWriter.h"

class SineWriter : public KeyValueWriter<SineWriter> {
public:
    explicit SineWriter(const Backend::SinePaletteConfig &palette);

    bool write(const std::string &filePath, std::string &err) const;

private:
    friend class KeyValueWriter<SineWriter>;

    inline static const std::string _headerLine = "Sine file";
    inline static const std::string _openFileError =
        "Failed to open sine file for writing.";
    inline static const std::string _writeFileError =
        "Failed while writing sine file.";

    void _writeBody(std::ostream &out) const override;

    Backend::SinePaletteConfig _palette;
};
