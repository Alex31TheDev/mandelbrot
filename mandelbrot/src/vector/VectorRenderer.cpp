#ifdef USE_VECTORS
#include "VectorRenderer.h"

#include <cstdint>

#include <immintrin.h>

#include "VectorTypes.h"
#include "../scalar/ScalarTypes.h"

#include "VectorGlobals.h"
#include "../scalar/ScalarGlobals.h"
using namespace VectorGlobals;
using namespace ScalarGlobals;

#include "../image/Image.h"
#include "VectorCoords.h"

#define FORMULA_VECTOR
#include "../formulas/FractalFormulas.h"

#include "../util/InlineUtil.h"

#define USE_VECTOR_STORE

static FORCE_INLINE void complexInverse_vec(simd_full_t &cr_vec, simd_full_t &ci_vec) {
    const simd_full_t cr2 = SIMD_MUL_F(cr_vec, cr_vec);
    const simd_full_t ci2 = SIMD_MUL_F(ci_vec, ci_vec);
    simd_full_t cmag = SIMD_ADD_F(cr2, ci2);

    const simd_full_mask_t mask = SIMD_CMP_NEQ_F(cmag, f_zero);
    cmag = SIMD_BLEND_F(f_one, cmag, mask);

    const simd_full_t inv_cmag = SIMD_DIV_F(f_one, cmag);

    cr_vec = SIMD_MUL_F(cr_vec, inv_cmag);
    ci_vec = SIMD_MUL_F(ci_vec, SIMD_MUL_F(inv_cmag, f_neg_one));
}

static FORCE_INLINE void initCoords_vec(simd_full_t &cr_vec, simd_full_t &ci_vec,
    simd_full_t &zr, simd_full_t &zi) {
    if (isInverse) {
        complexInverse_vec(cr_vec, ci_vec);
    }

    if (isJuliaSet) {
        zr = cr_vec;
        zi = ci_vec;

        cr_vec = f_seed_r_vec;
        ci_vec = f_seed_i_vec;
    } else {
        zr = f_seed_r_vec;
        zi = f_seed_i_vec;
    }
}

#ifdef USE_VECTOR_STORE

#include <array>

#include <emmintrin.h>
#include <tmmintrin.h>

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

static FORCE_INLINE void store128bitLane_vec(uint8_t *out, const __m128i &data, const int lane) {
    _mm_storeu_si128(
        reinterpret_cast<__m128i *>(out + lane * WIDTH_128_STRIDE),
        data
    );
}

static FORCE_INLINE void writePixelData_vec(uint8_t *out, const __m128i &RGBA8) {
    const __m128i mix = _mm_shuffle_epi8(RGBA8, rgbMask);
    store128bitLane_vec(out, mix, 0);
}

static FORCE_INLINE void writePixelData_vec(uint8_t *out, const __m256i &RGBA8) {
    const __m128i lanes[2] = {
        _mm256_castsi256_si128(RGBA8),
        _mm256_extracti128_si256(RGBA8, 1)
    };

    for (int i = 0; i < 2; i++) {
        const __m128i mix = _mm_shuffle_epi8(lanes[i], rgbMask);
        store128bitLane_vec(out, mix, i);
    }
}

static FORCE_INLINE void writePixelData_vec(uint8_t *out, const __m512i &RGBA8) {
    const __m128i lanes[4] = {
        _mm512_extracti32x4_epi32(RGBA8, 0),
        _mm512_extracti32x4_epi32(RGBA8, 1),
        _mm512_extracti32x4_epi32(RGBA8, 2),
        _mm512_extracti32x4_epi32(RGBA8, 3),
    };

    for (int i = 0; i < 4; i++) {
        const __m128i mix = _mm_shuffle_epi8(lanes[i], rgbMask);
        store128bitLane_vec(out, mix, i);
    }
}

#else
#include "../scalar/ScalarRenderer.h"
#endif

static FORCE_INLINE const simd_half_t normCos_vec(const simd_half_t &x) {
    const simd_half_t a = SIMD_COS_H(x);
    return SIMD_MUL_H(SIMD_ADD_H(a, h_one), h_half);
}

static FORCE_INLINE void getColorPixel_vec(const simd_half_t &val,
    simd_half_t &outR, simd_half_t &outG, simd_half_t &outB) {
    const simd_half_t R_x = SIMD_ADD_H(SIMD_MUL_H(val, h_freq_r_vec), h_phase_r_vec);
    const simd_half_t G_x = SIMD_ADD_H(SIMD_MUL_H(val, h_freq_g_vec), h_phase_g_vec);
    const simd_half_t B_x = SIMD_ADD_H(SIMD_MUL_H(val, h_freq_b_vec), h_phase_b_vec);

    outR = normCos_vec(R_x);
    outG = normCos_vec(G_x);
    outB = normCos_vec(B_x);
}

static FORCE_INLINE void setPixelsMasked_vec(uint8_t *pixels, int &pos,
    const int width, const simd_half_t &active,
    const simd_half_t &R, const simd_half_t &G, const simd_half_t &B) {
    const simd_half_mask_t inactive = SIMD_CMP_EQ_H(active, h_zero);

    const simd_half_t R_m = SIMD_AND_H(R, inactive);
    const simd_half_t G_m = SIMD_AND_H(G, inactive);
    const simd_half_t B_m = SIMD_AND_H(B, inactive);

    VectorRenderer::setPixels_vec(pixels, pos, width, R_m, G_m, B_m);
}

static FORCE_INLINE const simd_half_t getIterVal_vec(const simd_half_t &iter) {
#ifdef NORM_ITER_COUNT
    return SIMD_MUL_H(iter, h_invCount_vec);
#else
    return iter;
#endif
}

static FORCE_INLINE const simd_half_t getSmoothIterVal_vec(const simd_half_t &iter, const simd_half_t &mag) {
    const simd_half_t sqrt_mag = SIMD_SQRT_H(mag);
    const simd_half_t lg1 = SIMD_LOG_H(sqrt_mag);
    const simd_half_t m1 = SIMD_MUL_H(lg1, h_invLnBail_vec);
    const simd_half_t lg2 = SIMD_LOG_H(m1);
    const simd_half_t m2 = SIMD_MUL_H(lg2, h_invLnPow_vec);
    return SIMD_SUB_H(iter, m2);
}

static FORCE_INLINE const simd_half_t getLightVal_vec(const simd_half_t &zr, const simd_half_t &zi,
    const simd_half_t &dr, const simd_half_t &di) {
    const simd_half_t dr2 = SIMD_MUL_H(dr, dr);
    const simd_half_t di2 = SIMD_MUL_H(di, di);
    const simd_half_t dinv = SIMD_DIV_H(h_one, SIMD_ADD_H(dr2, di2));

    simd_half_t ur = SIMD_ADD_H(SIMD_MUL_H(zr, dr), SIMD_MUL_H(zi, di));
    simd_half_t ui = SIMD_SUB_H(SIMD_MUL_H(zi, dr), SIMD_MUL_H(zr, di));
    ur = SIMD_MUL_H(ur, dinv);
    ui = SIMD_MUL_H(ui, dinv);

    const simd_half_t ur2 = SIMD_MUL_H(ur, ur);
    const simd_half_t ui2 = SIMD_MUL_H(ui, ui);
    const simd_half_t umag = SIMD_DIV_H(h_one, SIMD_SQRT_H(SIMD_ADD_H(ur2, ui2)));
    ur = SIMD_MUL_H(ur, umag);
    ui = SIMD_MUL_H(ui, umag);

    const simd_half_t num = SIMD_ADD_H(
        SIMD_SUB_H(
            SIMD_MUL_H(ur, h_light_r_vec),
            SIMD_MUL_H(ui, h_light_i_vec)
        ),
        h_light_h_vec);
    const simd_half_t den = SIMD_ADD_H(h_light_h_vec, h_one);
    const simd_half_t light = SIMD_DIV_H(num, den);

#ifdef NORM_ITER_COUNT
    return SIMD_MUL_H(SIMD_MAX_H(light, h_zero), h_invCount_vec);
#else
    return SIMD_MAX_H(light, h_zero);
#endif
}

namespace VectorRenderer {
    FORCE_INLINE const simd_full_t iterateFractalSimd(const simd_full_t &cr, const simd_full_t &ci,
        simd_full_t &zr, simd_full_t &zi,
        simd_full_t &dr, simd_full_t &di,
        simd_full_t &mag, simd_full_mask_t &active) {
        simd_full_t iter = f_zero;
        mag = f_zero;

        active = SIMD_INIT_ONES_MASK_F;

        for (int i = 0; i < count; i++) {
            const simd_full_t zr2 = SIMD_MUL_F(zr, zr);
            const simd_full_t zi2 = SIMD_MUL_F(zi, zi);
            mag = SIMD_ADD_F(zr2, zi2);

            const simd_full_mask_t stillIterating = SIMD_CMP_LT_F(mag, f_bailout_vec);
            active = SIMD_AND_MASK_F(active, stillIterating);

            if (SIMD_MASK_F(active) == 0) break;

            switch (colorMethod) {
                case 2:
                {
                    simd_full_t new_dr, new_di;
                    derivative(zr, zi, dr, di, mag, new_dr, new_di);

                    dr = SIMD_BLEND_F(dr, new_dr, active);
                    di = SIMD_BLEND_F(di, new_di, active);
                }
                break;
            }

            simd_full_t new_zr, new_zi;
            formula(cr, ci, zr, zi, zr2, zi2, mag, new_zr, new_zi);

            iter = SIMD_ADD_MASK_F(iter, active, f_one);
            zr = SIMD_BLEND_F(zr, new_zr, active);
            zi = SIMD_BLEND_F(zi, new_zi, active);
        }

        return iter;
    }

    FORCE_INLINE const simd_half_int_t pixelToInt_vec(const simd_half_t &val) {
        simd_half_t conv_val = SIMD_MUL_H(val, h_255);
        conv_val = SIMD_MIN_H(SIMD_MAX_H(conv_val, h_zero), h_255);
        return SIMD_HALF_TO_INT32(conv_val);
    }

    FORCE_INLINE void setPixels_vec(uint8_t *pixels, int &pos,
        const int width, const simd_half_t &R, const simd_half_t &G, const simd_half_t &B) {
        const int byteCount = width * Image::STRIDE;
        uint8_t *out = pixels + pos;

#ifdef USE_VECTOR_STORE
        const simd_half_int_t R_i = pixelToInt_vec(R);
        const simd_half_int_t G_i = pixelToInt_vec(G);
        const simd_half_int_t B_i = pixelToInt_vec(B);

        const simd_half_int_t RG16 = SIMD_PACK_USAT_INT32_H(R_i, G_i);
        const simd_half_int_t BZ16 = SIMD_PACK_USAT_INT32_H(B_i, hi_zero);
        const simd_half_int_t RGBA8 = SIMD_PACK_USAT_INT16_H(RG16, BZ16);

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

    FORCE_INLINE void colorPixelsSimd(uint8_t *pixels, int &pos, const int width,
        const simd_full_t &iter, const simd_full_t &mag, const simd_full_mask_t &active,
        const simd_full_t &zr, const simd_full_t &zi,
        const simd_full_t &dr, const simd_full_t &di) {
        const simd_half_t h_active = SIMD_FULL_MASK_TO_HALF(active);

        simd_half_t vals, r_vec, g_vec, b_vec;
        r_vec = g_vec = b_vec = h_zero;

        switch (colorMethod) {
            case 0:
            {
                const simd_half_t h_iter = SIMD_FULL_TO_HALF(iter);

                vals = getIterVal_vec(h_iter);
                getColorPixel_vec(vals, r_vec, g_vec, b_vec);
            }
            break;

            case 1:
            {
                const simd_half_t h_iter = SIMD_FULL_TO_HALF(iter);
                const simd_half_t h_mag = SIMD_FULL_TO_HALF(mag);

                vals = getSmoothIterVal_vec(h_iter, h_mag);
                getColorPixel_vec(vals, r_vec, g_vec, b_vec);
            }
            break;

            case 2:
            {
                const simd_half_t h_zr = SIMD_FULL_TO_HALF(zr);
                const simd_half_t h_zi = SIMD_FULL_TO_HALF(zi);
                const simd_half_t h_dr = SIMD_FULL_TO_HALF(dr);
                const simd_half_t h_di = SIMD_FULL_TO_HALF(di);

                vals = getLightVal_vec(h_zr, h_zi, h_dr, h_di);
                r_vec = g_vec = b_vec = vals;
            }
            break;
        }

        setPixelsMasked_vec(pixels, pos, width, h_active, r_vec, g_vec, b_vec);
    }

    void renderPixelSimd(uint8_t *pixels, int &pos,
        const int width, const int x, scalar_full_t ci) {
        simd_full_t cr_vec = getCenterReal_vec(width, x);
        simd_full_t ci_vec = SIMD_SET_F(ci);

        simd_full_t zr, zi;
        simd_full_t dr = f_one, di = f_zero;

        initCoords_vec(cr_vec, ci_vec, zr, zi);

        simd_full_t mag;
        simd_full_mask_t active;

        simd_full_t iter = iterateFractalSimd(cr_vec, ci_vec, zr, zi, dr, di, mag, active);

        colorPixelsSimd(pixels, pos, width, iter, mag, active, zr, zi, dr, di);
    }
}

#endif