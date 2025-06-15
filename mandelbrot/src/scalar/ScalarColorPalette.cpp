#include "ScalarColorPalette.h"

#include <vector>

#include "ScalarTypes.h"

inline scalar_half_t lerp(
    scalar_half_t a, scalar_half_t b, scalar_half_t t
) {
    return a + (b - a) * t;
}

ScalarColorPalette::ScalarColorPalette(
    const std::vector<ScalarColor> &entries,
    scalar_half_t totalLength
) : _colors(entries), _totalLength(totalLength) {
    for (auto &c : _colors) {
        c.R = CLAMP_H(c.R, 0, 1);
        c.G = CLAMP_H(c.G, 0, 1);
        c.B = CLAMP_H(c.B, 0, 1);
    }

    if (_colors.empty() || NEG0_H(_totalLength)) {
        _totalLength = ONE_H;
        _invTotalLength = ZERO_H;
        return;
    } else {
        _invTotalLength = RECIP_H(_totalLength);
    }

    scalar_half_t lengthSum = ZERO_H;
    for (auto &col : _colors) lengthSum += col.length;

    if (POS_H(lengthSum)) {
        const scalar_half_t invSum = RECIP_H(lengthSum);
        for (auto &col : _colors) col.length *= invSum * _totalLength;
    }

    const size_t n = _colors.size();

    _accum.resize(n + 1);
    _accum[0] = ZERO_H;
    _inv.resize(n);

    for (size_t i = 0; i < n; ++i) {
        const scalar_half_t span = _colors[i].length;
        _accum[i + 1] = _accum[i] + span;
        _inv[i] = POS_H(span) ? RECIP_H(span) : ZERO_H;
    }

    _accum[n] = _totalLength;
}

ScalarColorPalette::_Segment
ScalarColorPalette::_locate(scalar_half_t x) const {
    if (_colors.empty()) return { 0, 0, ZERO_H };

    scalar_half_t t = x;

    if (NEG_H(t) || NEG0_H(_totalLength)) {
        t = FRAC_H(t);
    } else {
        t -= _totalLength * FLOOR_H(t * _invTotalLength);
    }

    if (t >= _totalLength) {
        t = NEXTAFTER_H(_totalLength, 0);
    }

    const size_t n = _colors.size();

    size_t idx = 0;
    while (idx + 1 < n && _accum[idx + 1] <= t) idx++;

    const scalar_half_t u = (t - _accum[idx]) * _inv[idx];
    const size_t next = (idx + 1) % n;

    return { idx, next, u };
}

ScalarColor ScalarColorPalette::sample(scalar_half_t x) const {
    if (_colors.empty()) {
        return { ZERO_H, ZERO_H, ZERO_H, ZERO_H };
    }

    const _Segment seg = _locate(x);

    const ScalarColor &col0 = _colors[seg.idx];
    const ScalarColor &col1 = _colors[seg.next];

    return {
        lerp(col0.R, col1.R, seg.u),
        lerp(col0.G, col1.G, seg.u),
        lerp(col0.B, col1.B, seg.u),
        ZERO_H
    };
}

void ScalarColorPalette::sample(scalar_half_t x,
    scalar_half_t &outR, scalar_half_t &outG, scalar_half_t &outB) const {
    if (_colors.empty()) {
        outR = outG = outB = ZERO_H;
        return;
    }

    const _Segment seg = _locate(x);

    const ScalarColor &col0 = _colors[seg.idx];
    const ScalarColor &col1 = _colors[seg.next];

    outR = lerp(col0.R, col1.R, seg.u);
    outG = lerp(col0.G, col1.G, seg.u);
    outB = lerp(col0.B, col1.B, seg.u);
}