#include "PaletteWriter.h"

#include <ostream>
#include <string>
#include <variant>
#include <type_traits>

#include "BackendAPI.h"

#include "util/FormatUtil.h"

PaletteWriter::PaletteWriter(const Backend::PaletteRGBConfig &palette)
    : _palette(palette) {}

PaletteWriter::PaletteWriter(const Backend::PaletteHexConfig &palette)
    : _palette(palette) {}

bool PaletteWriter::write(const std::string &filePath, std::string &err) const {
    return writeKeyValue(filePath, err);
}

void PaletteWriter::_writeBody(std::ostream &out) const {
    std::visit([&out](const auto &palette) {
        out << "totalLength=" << palette.totalLength << '\n'
            << "offset=" << palette.offset << '\n'
            << "blendEnds=" << FormatUtil::formatBool(palette.blendEnds) << '\n';

        using T = std::decay_t<decltype(palette)>;
        if constexpr (std::is_same_v<T, Backend::PaletteRGBConfig>) {
            for (const Backend::PaletteRGBEntry &entry : palette.entries) {
                out << "color="
                    << FormatUtil::formatHexColor(entry.R, entry.G, entry.B)
                    << " length=" << entry.length << '\n';
            }
        } else {
            for (const Backend::PaletteHexEntry &entry : palette.entries) {
                out << "color=" << entry.color
                    << " length=" << entry.length << '\n';
            }
        }
        }, _palette);
}