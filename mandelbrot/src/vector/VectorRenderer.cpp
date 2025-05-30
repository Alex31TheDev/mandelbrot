#ifdef USE_VECTORS
#define USE_VECTOR_STORE

#include "../scalar/ScalarTypes.h"
#include "VectorTypes.h"
#include "VectorRenderer.h"

#include <cstdint>
#include <array>

#include <immintrin.h>
#include <emmintrin.h>
#include <tmmintrin.h>

#include "../image/Image.h"

#include "../scalar/ScalarGlobals.h"
#include "VectorGlobals.h"

using namespace ScalarGlobals;
using namespace VectorGlobals;

#include "VectorCoords.h"

#ifdef USE_VECTOR_STORE

constexpr int WIDTH_128_STRIDE = 4 * Image::STRIDE;

template <int N, int total>
constexpr auto makeRgbMask() {
    std::array<int8_t, total> mask{};

    for (size_t i = 0; i < N; i++) {
        mask[i * 3] = CAST_INT_S(i, 8);
        mask[i * 3 + 1] = CAST_INT_S(i + 4, 8);
        mask[i * 3 + 2] = CAST_INT_S(i + 8, 8);
    }

    for (size_t i = N * 3; i < total; i++) mask[i] = -1;
    return mask;
}

static const __m128i rgbMask = _mm_loadu_si128(
    reinterpret_cast<const __m128i *>(makeRgbMask<4, 16>().data())
);

static inline void store128bitLane_vec(uint8_t *out, __m128i data, int lane) {
    _mm_storeu_si128(
        reinterpret_cast<__m128i *>(out + lane * WIDTH_128_STRIDE),
        data
    );
}

static inline void writePixelData_vec(uint8_t *out, __m128i RGBA8) {
    __m128i mix = _mm_shuffle_epi8(RGBA8, rgbMask);
    store128bitLane_vec(out, mix, 0);
}

static inline void writePixelData_vec(uint8_t *out, __m256i RGBA8) {
    __m128i lanes[2] = {
        _mm256_castsi256_si128(RGBA8),
        _mm256_extracti128_si256(RGBA8, 1)
    };

    for (int i = 0; i < 2; i++) {
        __m128i mix = _mm_shuffle_epi8(lanes[i], rgbMask);
        store128bitLane_vec(out, mix, i);
    }
}

static inline void writePixelData_vec(uint8_t *out, __m512i RGBA8) {
    __m128i lanes[4] = {
        _mm512_extracti32x4_epi32(RGBA8, 0),
        _mm512_extracti32x4_epi32(RGBA8, 1),
        _mm512_extracti32x4_epi32(RGBA8, 2),
        _mm512_extracti32x4_epi32(RGBA8, 3),
    };

    for (int i = 0; i < 4; i++) {
        __m128i mix = _mm_shuffle_epi8(lanes[i], rgbMask);
        store128bitLane_vec(out, mix, i);
    }
}

#else
#include "../scalar/ScalarRenderer.h"
#endif

static inline simd_half_t normCos_vec(simd_half_t x) {
    simd_half_t a = SIMD_COS_H(x);
    return SIMD_MUL_H(SIMD_ADD_H(a, h_one), h_half);
}

static inline void getColorPixel_vec(simd_half_t val,
    simd_half_t &outR, simd_half_t &outG, simd_half_t &outB) {
    simd_half_t R_x = SIMD_ADD_H(h_phase_r_vec, SIMD_MUL_H(val, h_freq_r_vec));
    simd_half_t G_x = SIMD_ADD_H(h_phase_g_vec, SIMD_MUL_H(val, h_freq_g_vec));
    simd_half_t B_x = SIMD_ADD_H(h_phase_b_vec, SIMD_MUL_H(val, h_freq_b_vec));

    outR = normCos_vec(R_x);
    outG = normCos_vec(G_x);
    outB = normCos_vec(B_x);
}

static inline void setPixelsMasked_vec(uint8_t *pixels, int &pos,
    int width, simd_half_t active, simd_half_t R, simd_half_t G, simd_half_t B) {
    simd_half_mask_t inactive = SIMD_CMP_EQ_H(active, h_zero);

    R = SIMD_AND_H(R, inactive);
    G = SIMD_AND_H(G, inactive);
    B = SIMD_AND_H(B, inactive);

    VectorRenderer::setPixels_vec(pixels, pos, width, R, G, B);
}

static inline simd_half_t getSmoothIterVal_vec(simd_half_t iter, simd_half_t mag) {
    simd_half_t sqrt_mag = SIMD_SQRT_H(mag);
    simd_half_t lg1 = SIMD_LOG_H(sqrt_mag);
    simd_half_t m1 = SIMD_MUL_H(lg1, h_invLnBail_vec);
    simd_half_t lg2 = SIMD_LOG_H(m1);
    simd_half_t m2 = SIMD_MUL_H(lg2, h_invLn2_vec);
    return SIMD_SUB_H(iter, m2);
}

static inline simd_half_t getLightVal_vec(simd_half_t zr, simd_half_t zi, simd_half_t dr, simd_half_t di) {
    simd_half_t dr2 = SIMD_MUL_H(dr, dr);
    simd_half_t di2 = SIMD_MUL_H(di, di);
    simd_half_t dinv = SIMD_DIV_H(h_one, SIMD_ADD_H(dr2, di2));

    simd_half_t ur = SIMD_ADD_H(SIMD_MUL_H(zr, dr), SIMD_MUL_H(zi, di));
    simd_half_t ui = SIMD_SUB_H(SIMD_MUL_H(zi, dr), SIMD_MUL_H(zr, di));
    ur = SIMD_MUL_H(ur, dinv);
    ui = SIMD_MUL_H(ui, dinv);

    simd_half_t ur2 = SIMD_MUL_H(ur, ur);
    simd_half_t ui2 = SIMD_MUL_H(ui, ui);
    simd_half_t umag = SIMD_DIV_H(h_one, SIMD_SQRT_H(SIMD_ADD_H(ur2, ui2)));
    ur = SIMD_MUL_H(ur, umag);
    ui = SIMD_MUL_H(ui, umag);

    simd_half_t num = SIMD_ADD_H(
        SIMD_ADD_H(
            SIMD_MUL_H(ur, h_light_r_vec),
            SIMD_MUL_H(ui, h_light_i_vec)),
        h_light_h_vec);
    simd_half_t den = SIMD_ADD_H(h_light_h_vec, h_one);

    simd_half_t light = SIMD_DIV_H(num, den);
    return SIMD_MAX_H(light, h_zero);
}

namespace VectorRenderer {
    inline simd_half_int_t pixelToInt_vec(simd_half_t val) {
        val = SIMD_MUL_H(val, h_255);
        val = SIMD_MIN_H(SIMD_MAX_H(val, h_zero), h_255);
        return SIMD_HALF_TO_INT32(val);
    }

    inline void setPixels_vec(uint8_t *pixels, int &pos,
        int width, simd_half_t R, simd_half_t G, simd_half_t B) {
        int byteCount = width * Image::STRIDE;
        uint8_t *out = pixels + pos;

#ifdef USE_VECTOR_STORE
        simd_half_int_t R_i = pixelToInt_vec(R);
        simd_half_int_t G_i = pixelToInt_vec(G);
        simd_half_int_t B_i = pixelToInt_vec(B);

        simd_half_int_t RG16 = SIMD_PACK_USAT_INT32_H(R_i, G_i);
        simd_half_int_t BZ16 = SIMD_PACK_USAT_INT32_H(B_i, hi_zero);
        simd_half_int_t RGBA8 = SIMD_PACK_USAT_INT16_H(RG16, BZ16);

        writePixelData_vec(out, RGBA8);
#else
        alignas(SIMD_HALF_ALIGNMENT) scalar_half_t R_arr[SIMD_HALF_WIDTH];
        alignas(SIMD_HALF_ALIGNMENT) scalar_half_t G_arr[SIMD_HALF_WIDTH];
        alignas(SIMD_HALF_ALIGNMENT) scalar_half_t B_arr[SIMD_HALF_WIDTH];

        SIMD_STORE_H(R_arr, R);
        SIMD_STORE_H(G_arr, G);
        SIMD_STORE_H(B_arr, B);

        for (size_t i = 0; i < width; i++) {
            out[i * 3] = ScalarRenderer::pixelToInt(R_arr[i]);
            out[i * 3 + 1] = ScalarRenderer::pixelToInt(G_arr[i]);
            out[i * 3 + 2] = ScalarRenderer::pixelToInt(B_arr[i]);
        }
#endif

        pos += byteCount;
    }

    void renderPixelSimd(uint8_t *pixels, int &pos,
        int width, int x, scalar_full_t ci) {
        simd_full_t cr_vec = getCenterReal(width, x);
        simd_full_t ci_vec = SIMD_SET_F(ci);

        if (isInverse) {
            simd_full_t cr2 = SIMD_MUL_F(cr_vec, cr_vec);
            simd_full_t ci2 = SIMD_MUL_F(ci_vec, ci_vec);
            simd_full_t cmag = SIMD_ADD_F(cr2, ci2);

            simd_full_mask_t mask = SIMD_CMP_NEQ_F(cmag, f_zero);
            cmag = SIMD_BLEND_F(f_one, cmag, mask);

            simd_full_t inv_cmag = SIMD_DIV_F(f_one, cmag);

            cr_vec = SIMD_MUL_F(cr_vec, inv_cmag);
            ci_vec = SIMD_MUL_F(ci_vec, SIMD_MUL_F(inv_cmag, f_neg_one));
        }

        simd_full_t zr, zi;
        simd_full_t dr = f_one, di = f_zero;

        if (isJuliaSet) {
            zr = cr_vec;
            zi = ci_vec;

            cr_vec = f_seed_r_vec;
            ci_vec = f_seed_i_vec;
        } else {
            zr = f_seed_r_vec;
            zi = f_seed_i_vec;
        }

        simd_full_t iter = f_zero;
        simd_full_t mag = f_zero;

        simd_full_mask_t active = SIMD_INIT_ONES_MASK_F;

        for (int i = 0; i < ScalarGlobals::count; i++) {
            simd_full_t zr2 = SIMD_MUL_F(zr, zr);
            simd_full_t zi2 = SIMD_MUL_F(zi, zi);
            mag = SIMD_ADD_F(zr2, zi2);

            simd_full_mask_t stillIterating = SIMD_CMP_LT_F(mag, f_bailout_vec);
            active = SIMD_AND_MASK_F(active, stillIterating);

            if (SIMD_MASK_F(active) == 0) break;

            switch (ScalarGlobals::colorMethod) {
                case 1:
                {
                    simd_full_t t1 = SIMD_SUB_F(SIMD_MUL_F(zr, dr), SIMD_MUL_F(zi, di));
                    simd_full_t t2 = SIMD_ADD_F(SIMD_MUL_F(zr, di), SIMD_MUL_F(zi, dr));

                    simd_full_t new_dr = SIMD_ADD_F(SIMD_ADD_F(t1, t1), f_one);
                    simd_full_t new_di = SIMD_ADD_F(t2, t2);

                    dr = SIMD_BLEND_F(dr, new_dr, active);
                    di = SIMD_BLEND_F(di, new_di, active);
                }
                break;
            }

            simd_full_t zrzi = SIMD_MUL_F(zr, zi);
            simd_full_t new_zi = SIMD_ADD_F(SIMD_ADD_F(zrzi, zrzi), ci_vec);
            simd_full_t new_zr = SIMD_ADD_F(SIMD_SUB_F(zr2, zi2), cr_vec);

            iter = SIMD_ADD_MASK_F(iter, active, f_one);
            zr = SIMD_BLEND_F(zr, new_zr, active);
            zi = SIMD_BLEND_F(zi, new_zi, active);
        }

        simd_half_t h_active = SIMD_FULL_MASK_TO_HALF(active);

        simd_half_t r_vec, g_vec, b_vec;
        r_vec = g_vec = b_vec = h_zero;

        switch (ScalarGlobals::colorMethod) {
            case 0:
            {
                simd_half_t h_iter = SIMD_FULL_TO_HALF(iter);
                simd_half_t h_mag = SIMD_FULL_TO_HALF(mag);

                simd_half_t vals = getSmoothIterVal_vec(h_iter, h_mag);
                getColorPixel_vec(vals, r_vec, g_vec, b_vec);
            }
            break;

            case 1:
            {
                simd_half_t h_zr = SIMD_FULL_TO_HALF(zr);
                simd_half_t h_zi = SIMD_FULL_TO_HALF(zi);
                simd_half_t h_dr = SIMD_FULL_TO_HALF(dr);
                simd_half_t h_di = SIMD_FULL_TO_HALF(di);

                simd_half_t vals = getLightVal_vec(h_zr, h_zi, h_dr, h_di);
                r_vec = g_vec = b_vec = vals;
            }
            break;
        }

        setPixelsMasked_vec(pixels, pos, width, h_active, r_vec, g_vec, b_vec);
    }
}

#endif