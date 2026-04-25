#include "CommonDefs.h"
#include "ScalarSinePalette.h"

ScalarSinePalette::ScalarSinePalette(
    scalar_half_t freqR,
    scalar_half_t freqG,
    scalar_half_t freqB,
    scalar_half_t freqMult,
    scalar_half_t phaseR,
    scalar_half_t phaseG,
    scalar_half_t phaseB,
    scalar_half_t cosPhase
) {
    _freq_r = freqR * freqMult;
    _freq_g = freqG * freqMult;
    _freq_b = freqB * freqMult;

    _phase_r = phaseR + cosPhase;
    _phase_g = phaseG + cosPhase;
    _phase_b = phaseB + cosPhase;
}