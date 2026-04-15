#pragma once

#include <string>
#include <string_view>

#include "ScalarTypes.h"

struct ScalarColor {
    scalar_half_t R, G, B;

    static ScalarColor fromString(std::string_view hex);
};

struct ScalarPaletteColor : public ScalarColor {
    scalar_half_t length;

    static ScalarColor fromHex(std::string_view) = delete;

    static ScalarPaletteColor fromString(
        std::string_view hex, const std::string &lengthStr
    );
};

#if USE_SCALAR_COLORING
#include "util/InlineUtil.h"

#define lerp(a, b, t) ((a) + ((b) - (a)) * (t))

FORCE_INLINE ScalarColor colorLerp(
    const ScalarColor &a, const ScalarColor &b,
    scalar_half_t t
) {
    return {
        lerp(a.R, b.R, t),
        lerp(a.G, b.G, t),
        lerp(a.B, b.B, t)
    };
}

#endif
