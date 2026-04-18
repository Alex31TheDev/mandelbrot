#include "PaletteWriter.h"

#include <fstream>
#include <string>
#include <type_traits>

#include "util/FormatUtil.h"

PaletteWriter::PaletteWriter(const Backend::PaletteRGBConfig &palette)
    : _palette(palette) {}

PaletteWriter::PaletteWriter(const Backend::PaletteHexConfig &palette)
    : _palette(palette) {}

bool PaletteWriter::write(const std::string &filePath, std::string &err) const {
    err.clear();

    std::ofstream fout(filePath);
    if (!fout.is_open()) {
        err = "Failed to open palette file for writing.";
        return false;
    }

    fout << "# Palette file\n";
    std::visit([&fout](const auto &palette) {
        fout << "totalLength=" << palette.totalLength << '\n'
            << " offset=" << palette.offset << '\n';

        using PaletteT = std::decay_t<decltype(palette)>;
        if constexpr (std::is_same_v<PaletteT, Backend::PaletteRGBConfig>) {
            for (const Backend::PaletteRGBEntry &entry : palette.entries) {
                fout << "color="
                    << FormatUtil::formatHexColor(entry.R, entry.G, entry.B)
                    << " length=" << entry.length << '\n';
            }
        } else {
            for (const Backend::PaletteHexEntry &entry : palette.entries) {
                fout << "color=" << entry.color
                    << " length=" << entry.length << '\n';
            }
        }
        }, _palette);

    if (!fout.good()) {
        err = "Failed while writing palette file.";
        return false;
    }

    return true;
}