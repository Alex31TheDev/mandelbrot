#pragma once
#ifdef USE_VECTORS
#include "CommonDefs.h"

#include "VectorTypes.h"

#include "../scalar/ScalarSinePalette.h"

class VectorSinePalette {
public:
    explicit VectorSinePalette(const ScalarSinePalette &palette);

    FORCE_INLINE simd_half_t getFreqR() const { return _freq_r_vec; }
    FORCE_INLINE simd_half_t getFreqG() const { return _freq_g_vec; }
    FORCE_INLINE simd_half_t getFreqB() const { return _freq_b_vec; }

    FORCE_INLINE simd_half_t getPhaseR() const { return _phase_r_vec; }
    FORCE_INLINE simd_half_t getPhaseG() const { return _phase_g_vec; }
    FORCE_INLINE simd_half_t getPhaseB() const { return _phase_b_vec; }

private:
    simd_half_t _freq_r_vec;
    simd_half_t _freq_g_vec;
    simd_half_t _freq_b_vec;

    simd_half_t _phase_r_vec;
    simd_half_t _phase_g_vec;
    simd_half_t _phase_b_vec;
};

#endif
