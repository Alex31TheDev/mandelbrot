#ifdef USE_VECTORS
#include "CommonDefs.h"
#include "VectorSinePalette.h"

VectorSinePalette::VectorSinePalette(const ScalarSinePalette &palette) {
    _freq_r_vec = SIMD_SET1_H(palette._freq_r);
    _freq_g_vec = SIMD_SET1_H(palette._freq_g);
    _freq_b_vec = SIMD_SET1_H(palette._freq_b);

    _phase_r_vec = SIMD_SET1_H(palette._phase_r);
    _phase_g_vec = SIMD_SET1_H(palette._phase_g);
    _phase_b_vec = SIMD_SET1_H(palette._phase_b);
}

#endif