#pragma once
#include "CommonDefs.h"

#include "ScalarTypes.h"

class VectorSinePalette;

class ScalarSinePalette {
public:
    explicit ScalarSinePalette(scalar_half_t freqR,
        scalar_half_t freqG,
        scalar_half_t freqB,
        scalar_half_t freqMult,
        scalar_half_t phaseR,
        scalar_half_t phaseG,
        scalar_half_t phaseB,
        scalar_half_t cosPhase);

    FORCE_INLINE scalar_half_t getFreqR() const { return _freq_r; }
    FORCE_INLINE scalar_half_t getFreqG() const { return _freq_g; }
    FORCE_INLINE scalar_half_t getFreqB() const { return _freq_b; }

    FORCE_INLINE scalar_half_t getPhaseR() const { return _phase_r; }
    FORCE_INLINE scalar_half_t getPhaseG() const { return _phase_g; }
    FORCE_INLINE scalar_half_t getPhaseB() const { return _phase_b; }

private:
    scalar_half_t _freq_r = ZERO_H;
    scalar_half_t _freq_g = ZERO_H;
    scalar_half_t _freq_b = ZERO_H;

    scalar_half_t _phase_r = ZERO_H;
    scalar_half_t _phase_g = ZERO_H;
    scalar_half_t _phase_b = ZERO_H;

    friend class VectorSinePalette;
};
