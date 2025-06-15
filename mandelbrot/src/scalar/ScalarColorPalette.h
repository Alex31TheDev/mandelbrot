#pragma once

#include <vector>

#include "ScalarTypes.h"

struct ScalarColor {
    scalar_half_t R, G, B;
    scalar_half_t length;
};

class ScalarColorPalette {
public:
    ScalarColorPalette(const std::vector<ScalarColor> &entries,
        scalar_half_t totalLength = ONE_H);

    ScalarColor sample(scalar_half_t x) const;
    void sample(scalar_half_t x,
        scalar_half_t &outR, scalar_half_t &outG, scalar_half_t &outB) const;

private:
    scalar_half_t _totalLength;
    scalar_half_t _invTotalLength;

    std::vector<ScalarColor> _colors;

    std::vector<scalar_half_t> _accum;
    std::vector<scalar_half_t> _inv;

    struct _Segment {
        size_t idx, next;
        scalar_half_t u;
    };

    _Segment _locate(scalar_half_t x) const;
};