#ifdef USE_VECTORS
#include "CommonDefs.h"
#include "VectorGlobals.h"

#include "VectorTypes.h"

#include "../scalar/ScalarGlobals.h"
using namespace ScalarGlobals;

namespace VectorGlobals {
    simd_full_t f_coordRealScale_vec, f_coordImagScale_vec;
    simd_full_t f_coordRealOffset_vec, f_coordImagOffset_vec;

    simd_full_t f_seed_r_vec, f_seed_i_vec;
    simd_full_t startIterVal;

    simd_half_t h_invCount_vec, h_invLnPow_vec;

    simd_half_t h_freq_r_vec, h_freq_g_vec, h_freq_b_vec;
    simd_half_t h_phase_r_vec, h_phase_g_vec, h_phase_b_vec;
    simd_half_t h_light_r_vec, h_light_i_vec, h_light_h_vec;

    static void _calcCoordConsts() {
        const simd_full_t halfWidth_vec = SIMD_SET1_F(halfWidth);
        const simd_full_t halfHeight_vec = SIMD_SET1_F(halfHeight);

        const simd_full_t invWidth_vec = SIMD_SET1_F(invWidth);
        const simd_full_t invHeight_vec = SIMD_SET1_F(invHeight);

        const simd_full_t realScale_vec = SIMD_SET1_F(realScale);
        const simd_full_t imagScale_vec = SIMD_SET1_F(imagScale);

        const simd_full_t point_r_vec = SIMD_SET1_F(point_r);
        const simd_full_t point_i_vec = SIMD_SET1_F(point_i);

        f_coordRealScale_vec = SIMD_MUL_F(invWidth_vec, realScale_vec);
        f_coordImagScale_vec = SIMD_MUL_F(invHeight_vec, imagScale_vec);

        f_coordRealOffset_vec = SIMD_SUBMUL_F(
            halfWidth_vec, f_coordRealScale_vec,
            point_r_vec
        );
        f_coordImagOffset_vec = SIMD_NMULADD_F(
            halfHeight_vec, f_coordImagScale_vec,
            point_i_vec
        );
    }

    void initVectors() {
        _calcCoordConsts();

        f_seed_r_vec = SIMD_SET1_F(seed_r);
        f_seed_i_vec = SIMD_SET1_F(seed_i);

        startIterVal = normalSeed ? SIMD_ONE_F : SIMD_ZERO_F;

        h_invCount_vec = SIMD_SET1_H(invCount);
        h_invLnPow_vec = SIMD_SET1_H(invLnPow);

        h_freq_r_vec = SIMD_SET1_H(freq_r);
        h_freq_g_vec = SIMD_SET1_H(freq_g);
        h_freq_b_vec = SIMD_SET1_H(freq_b);

        h_phase_r_vec = SIMD_SET1_H(phase_r);
        h_phase_g_vec = SIMD_SET1_H(phase_g);
        h_phase_b_vec = SIMD_SET1_H(phase_b);

        h_light_r_vec = SIMD_SET1_H(light_r);
        h_light_i_vec = SIMD_SET1_H(light_i);
        h_light_h_vec = SIMD_SET1_H(light_h);
    }
}

#endif