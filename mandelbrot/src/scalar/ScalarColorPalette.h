#pragma once

#include <vector>

#include "ScalarTypes.h"

struct ScalarColor {
    scalar_half_t R, G, B;
    scalar_half_t length;
};

class VectorColorPalette;

class ScalarColorPalette {
public:
    static inline scalar_half_t lerp(
        scalar_half_t a, scalar_half_t b, scalar_half_t t
    ) {
        return a + (b - a) * t;
    }

    static inline ScalarColor lerp(
        const ScalarColor &a,
        const ScalarColor &b,
        scalar_half_t t
    ) {
        return {
            lerp(a.R, b.R, t),
            lerp(a.G, b.G, t),
            lerp(a.B, b.B, t),
            ZERO_H
        };
    }

    ScalarColorPalette(const std::vector<ScalarColor> &entries,
        scalar_half_t totalLength = ONE_H, scalar_half_t offset = ZERO_H,
        bool blendEnds = true);

    [[nodiscard]] const std::vector<ScalarColor> &
        getEntries() const { return _colors; }
    [[nodiscard]] scalar_half_t getTotalLength() const { return _totalLength; }

    ScalarColor sample(scalar_half_t x) const;
    void sample(scalar_half_t x,
        scalar_half_t &outR, scalar_half_t &outG, scalar_half_t &outB) const;

private:
    size_t _numSegments = 0;
    bool _blendEnds;

    scalar_half_t _totalLength, _invLength;
    scalar_half_t _offset = ZERO_H, _epsilon = ZERO_H;

    std::vector<ScalarColor> _colors{};
    std::vector<scalar_half_t> _accum{}, _inv{};

    struct _Segment {
        size_t idx, next;
        scalar_half_t u;
    };

    _Segment _locate(scalar_half_t x) const;

    friend class VectorColorPalette;
};