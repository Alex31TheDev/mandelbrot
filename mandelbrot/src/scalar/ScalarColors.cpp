#include "ScalarColors.h"

#include <string>
#include <string_view>

#include "ScalarTypes.h"

#include "../util/ParserUtil.h"
using namespace ParserUtil;

static void parseColor(std::string_view hex,
    scalar_half_t &r, scalar_half_t &g, scalar_half_t &b) {
    bool ok;
    auto [r_i, g_i, b_i] = parseHexString(hex, &ok);

    if (ok) {
        r = CAST_H(r_i) / SC_SYM_H(255.0);
        g = CAST_H(g_i) / SC_SYM_H(255.0);
        b = CAST_H(b_i) / SC_SYM_H(255.0);
    } else {
        r = g = b = NEGONE_H;
    }
}

ScalarColor ScalarColor::fromString(std::string_view hex) {
    ScalarColor out;
    parseColor(hex, out.R, out.G, out.B);
    return out;
}

ScalarPaletteColor ScalarPaletteColor::fromString(
    std::string_view hex, const std::string &lengthStr
) {
    ScalarPaletteColor out;
    parseColor(hex, out.R, out.G, out.B);

    out.length = parseNumber(lengthStr, NEGONE_H);
    return out;
}