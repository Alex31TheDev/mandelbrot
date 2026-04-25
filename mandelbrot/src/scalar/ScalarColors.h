#pragma once

#include <string>
#include <string_view>

#include "ScalarTypes.h"
#include "util/InlineUtil.h"

struct ScalarColor {
    scalar_half_t R, G, B;

    static ScalarColor fromString(std::string_view hex);

    bool valid() const;
};

struct ScalarPaletteColor : public ScalarColor {
    scalar_half_t length;

    static ScalarColor fromHex(std::string_view) = delete;
    static ScalarPaletteColor fromString(std::string_view hex,
        const std::string &lengthStr);

    bool valid() const;
};

FORCE_INLINE uint8_t toColorByte(scalar_half_t value) {
    const scalar_half_t clamped = CLAMP_H(value, ZERO_H, ONE_H);
    return static_cast<uint8_t>(clamped * SC_SYM_H(255.0) + ONEHALF_H);
}

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
