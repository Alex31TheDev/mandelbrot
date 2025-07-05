#pragma once
#include "CommonDefs.h"

#include <vector>

#include "ScalarTypes.h"
#include "ScalarColors.h"

class VectorColorPalette;

class ScalarColorPalette {
public:
#if USE_SCALAR_COLORING

    static inline scalar_half_t lerp(
        scalar_half_t a, scalar_half_t b, scalar_half_t t
    ) {
        return a + (b - a) * t;
    }

    static inline ScalarColor lerp(
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

    explicit ScalarColorPalette(const std::vector<ScalarPaletteColor> &entries,
        scalar_half_t totalLength = ONE_H, scalar_half_t offset = ZERO_H,
        bool blendEnds = true);

    [[nodiscard]] const std::vector<ScalarPaletteColor> &
        getEntries() const { return _colors; }
    [[nodiscard]] scalar_half_t getTotalLength() const { return _totalLength; }

#if USE_SCALAR_COLORING
    ScalarColor sample(scalar_half_t x) const;
    void sample(scalar_half_t x,
        scalar_half_t &outR, scalar_half_t &outG, scalar_half_t &outB) const;
#endif

private:
    size_t _numSegments = 0;
    bool _blendEnds;

    scalar_half_t _totalLength, _invLength;
    scalar_half_t _offset = ZERO_H, _epsilon = ZERO_H;

    std::vector<ScalarPaletteColor> _colors;
    std::vector<scalar_half_t> _accum, _inv;

#if USE_SCALAR_COLORING
    struct _Segment {
        size_t idx, next;
        scalar_half_t u;
    };
    _Segment _locate(scalar_half_t x) const;
#endif

    friend class VectorColorPalette;
};