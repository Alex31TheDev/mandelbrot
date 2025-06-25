#include "CommonDefs.h"
#include "ScalarColorPalette.h"

#include <vector>

#include "ScalarTypes.h"
#include "ScalarColors.h"

ScalarColorPalette::ScalarColorPalette(
    const std::vector<ScalarPaletteColor> &entries,
    scalar_half_t totalLength, scalar_half_t offset,
    bool blendEnds
) : _blendEnds(blendEnds), _totalLength(totalLength) {
    if (entries.empty() || ISNEG0_H(_totalLength)) {
        _totalLength = ONE_H;
        _invLength = ZERO_H;
        return;
    }

    const size_t n = entries.size();
    _numSegments = n;
    if (!_blendEnds) _numSegments--;

    if (_numSegments == 0) {
        _totalLength = ONE_H;
        _invLength = ZERO_H;
        return;
    }

    _colors.resize(n);

    for (size_t i = 0; i < n; i++) {
        _colors[i].R = CLAMP_H(entries[i].R, 0, 1);
        _colors[i].G = CLAMP_H(entries[i].G, 0, 1);
        _colors[i].B = CLAMP_H(entries[i].B, 0, 1);
        _colors[i].length = MAX_H(entries[i].length, 0);
    }

    scalar_half_t lengthSum = ZERO_H;
    for (size_t i = 0; i < _numSegments; i++) lengthSum += _colors[i].length;

    if (ISNOT0_H(lengthSum)) {
        const scalar_half_t invSum = RECIP_H(lengthSum);
        for (auto &col : _colors) col.length *= invSum * _totalLength;
    }

    _accum.resize(_numSegments + 1);
    _accum[0] = ZERO_H;
    _inv.resize(_numSegments);

    for (size_t i = 0; i < _numSegments; i++) {
        const scalar_half_t span = _colors[i].length;
        _accum[i + 1] = _accum[i] + span;
        _inv[i] = ISPOS_H(span) ? RECIP_H(span) : ZERO_H;
    }

    if (!_blendEnds) _totalLength = _accum[_numSegments];
    _invLength = ISPOS_H(_totalLength) ? RECIP_H(_totalLength) : ZERO_H;

    _offset = CLAMP_H(offset, 0, _totalLength);
    _epsilon = NEXTAFTER_H(_totalLength, 0);
}

ScalarColorPalette::_Segment
ScalarColorPalette::_locate(scalar_half_t x) const {
    if (_numSegments == 0) return { 0, 0, ZERO_H };

    const scalar_half_t inVal = x + _offset;
    scalar_half_t t = inVal - _totalLength * FLOOR_H(inVal * _invLength);
    if (ISNEG_H(t)) t += _totalLength;
    t = MIN_H(t, _epsilon);

    size_t idx = 0;
    while (idx + 1 < _numSegments && _accum[idx + 1] <= t) idx++;

    const scalar_half_t u = (t - _accum[idx]) * _inv[idx];
    const size_t next = _blendEnds ? (idx + 1) % _colors.size() : idx + 1;

    return { idx, next, u };
}

ScalarColor ScalarColorPalette::sample(scalar_half_t x) const {
    if (_colors.empty()) {
        return { ZERO_H, ZERO_H, ZERO_H };
    }

    const _Segment seg = _locate(x);

    const ScalarColor &color1 = _colors[seg.idx];
    const ScalarColor &color2 = _colors[seg.next];

    return lerp(color1, color2, seg.u);
}

void ScalarColorPalette::sample(
    scalar_half_t x,
    scalar_half_t &outR, scalar_half_t &outG, scalar_half_t &outB
) const {
    if (_colors.empty()) {
        outR = outG = outB = ZERO_H;
        return;
    }

    const _Segment seg = _locate(x);

    const ScalarColor &color1 = _colors[seg.idx];
    const ScalarColor &color2 = _colors[seg.next];

    outR = lerp(color1.R, color2.R, seg.u);
    outG = lerp(color1.G, color2.G, seg.u);
    outB = lerp(color1.B, color2.B, seg.u);
}