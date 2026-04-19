#include "SineWriter.h"

#include <ostream>
#include <string>

SineWriter::SineWriter(const Backend::SinePaletteConfig &palette)
    : _palette(palette) {}

bool SineWriter::write(const std::string &filePath, std::string &err) const {
    return writeKeyValue(filePath, err);
}

void SineWriter::_writeBody(std::ostream &out) const {
    out << "freqR=" << _palette.freqR << '\n'
        << "freqG=" << _palette.freqG << '\n'
        << "freqB=" << _palette.freqB << '\n'
        << "freqMult=" << _palette.freqMult << '\n';
}
