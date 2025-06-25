#include "ScalarColor.h"

#include <string_view>

#include "ScalarTypes.h"

#include "../util/ParserUtil.h"
using namespace ParserUtil;

static void parseColor(std::string_view hex,
    scalar_half_t &r, scalar_half_t &g, scalar_half_t &b) {
    bool ok;
    auto [r, g, b] = parseHexString(hex, &ok);

    if (ok) {
        r = CAST_H(r) / SC_SYM_H(255.0);
        g = CAST_H(g) / SC_SYM_H(255.0);
        b = CAST_H(b) / SC_SYM_H(255.0);
    } else {
        r = g = b = NEG_ONE_H;
    }
}

ScalarColor ScalarColor::fromString(std::string_view hex) {
    ScalarColor out;
    parseColor(hex, out.R, out.G, out.B);
    return out;
}

ScalarPaletteColor ScalarPaletteColor::fromString(
    std::string_view hex, std::string_view lengthStr
) {
    ScalarPaletteColor out;
    parseColor(hex, out.R, out.G, out.B);

    out.length = parseNumber(std::string(lengthStr), NEG_ONE_H);
}