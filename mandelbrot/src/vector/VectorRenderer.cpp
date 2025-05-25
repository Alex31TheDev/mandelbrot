#include "VectorRenderer.h"

#include <cstdint>
#include <cstring>

#include <immintrin.h>
#include <emmintrin.h>
#include <tmmintrin.h>

#include "../image/Image.h"

#include "../scalar/ScalarGlobals.h"
#include "VectorGlobals.h"

using namespace ScalarGlobals;
using namespace VectorGlobals;

#include "VectorCoords.h"

static const __m128i byteMask = _mm_setr_epi8(
    0, 4, 8, 12,
    -1, -1, -1, -1,
    -1, -1, -1, -1,
    -1, -1, -1, -1
);

static const __m128i rgbMask = _mm_setr_epi8(
    0, 1, 8,
    2, 3, 9,
    4, 5, 10,
    6, 7, 11,
    -1, -1, -1, -1
);

static inline __m128 normCos_vec(__m128 x) {
    __m128 a = _mm_cos_ps(x);
    return _mm_mul_ps(_mm_add_ps(a, f_one), f_half);
}

static inline void getColorPixel_vec(__m128 val,
    __m128 &outR, __m128 &outG, __m128 &outB) {
    __m128 R_x = _mm_add_ps(f_phase_r_vec, _mm_mul_ps(val, f_freq_r_vec));
    __m128 G_x = _mm_add_ps(f_phase_g_vec, _mm_mul_ps(val, f_freq_g_vec));
    __m128 B_x = _mm_add_ps(f_phase_b_vec, _mm_mul_ps(val, f_freq_b_vec));

    outR = normCos_vec(R_x);
    outG = normCos_vec(G_x);
    outB = normCos_vec(B_x);
}

static inline __m128i pixelToInt_vec(__m128 val) {
    val = _mm_mul_ps(val, f_255);
    val = _mm_min_ps(_mm_max_ps(val, f_zero), f_255);
    return _mm_cvtps_epi32(val);
}

static inline void setPixelsMasked_vec(uint8_t *pixels, int &pos,
    int width, __m128 active, __m128 R, __m128 G, __m128 B) {
    __m128 inactive = _mm_cmpeq_ps(active, f_zero);
    R = _mm_and_ps(R, inactive);
    G = _mm_and_ps(G, inactive);
    B = _mm_and_ps(B, inactive);

    VectorRenderer::setPixels_vec(pixels, pos, width, R, G, B);
}

static inline __m128 getSmoothIterVal_vec(__m128 iter, __m128 mag) {
    __m128 sqrt_mag = _mm_sqrt_ps(mag);
    __m128 lg1 = _mm_log_ps(sqrt_mag);
    __m128 m1 = _mm_mul_ps(lg1, f_invLnBail_vec);
    __m128 lg2 = _mm_log_ps(m1);
    __m128 m2 = _mm_mul_ps(lg2, f_invLn2_vec);
    return _mm_sub_ps(iter, m2);
}

static inline __m128 getLightVal_vec(__m128 zr, __m128 zi, __m128 dr, __m128 di) {
    __m128 dr2 = _mm_mul_ps(dr, dr);
    __m128 di2 = _mm_mul_ps(di, di);
    __m128 dinv = _mm_div_ps(f_one, _mm_add_ps(dr2, di2));

    __m128 ur = _mm_add_ps(_mm_mul_ps(zr, dr), _mm_mul_ps(zi, di));
    __m128 ui = _mm_sub_ps(_mm_mul_ps(zi, dr), _mm_mul_ps(zr, di));
    ur = _mm_mul_ps(ur, dinv);
    ui = _mm_mul_ps(ui, dinv);

    __m128 ur2 = _mm_mul_ps(ur, ur);
    __m128 ui2 = _mm_mul_ps(ui, ui);
    __m128 umag = _mm_div_ps(f_one, _mm_sqrt_ps(_mm_add_ps(ur2, ui2)));
    ur = _mm_mul_ps(ur, umag);
    ui = _mm_mul_ps(ui, umag);

    __m128 num = _mm_add_ps(
        _mm_add_ps(
            _mm_mul_ps(ur, f_light_r_vec),
            _mm_mul_ps(ui, f_light_i_vec)
        ),
        f_light_h_vec
    );
    __m128 den = _mm_add_ps(f_light_h_vec, f_one);

    __m128 light = _mm_div_ps(num, den);
    return _mm_max_ps(light, f_zero);
}

namespace VectorRenderer {
    inline void setPixels_vec(uint8_t *pixels, int &pos,
        int width, __m128 R, __m128 G, __m128 B) {
        __m128i R_i = pixelToInt_vec(R);
        __m128i G_i = pixelToInt_vec(G);
        __m128i B_i = pixelToInt_vec(B);

        __m128i R_8 = _mm_shuffle_epi8(R_i, byteMask);
        __m128i G_8 = _mm_shuffle_epi8(G_i, byteMask);
        __m128i B_8 = _mm_shuffle_epi8(B_i, byteMask);

        __m128i RG = _mm_unpacklo_epi8(R_8, G_8);
        __m128i mix = _mm_unpacklo_epi64(RG, B_8);
        __m128i rgb12 = _mm_shuffle_epi8(mix, rgbMask);

        int byteCount = width * Image::STRIDE;
        uint8_t *out = pixels + pos;

        if (width == SIMD_WIDTH) {
            _mm_store_si128(reinterpret_cast<__m128i *>(out), rgb12);
        } else {
            alignas(Image::ALIGNMENT) uint8_t rgb_bytes[16];
            _mm_store_si128(reinterpret_cast<__m128i *>(rgb_bytes), rgb12);
            memcpy(out, rgb_bytes, byteCount);
        }

        pos += byteCount;
    }

    void renderPixelSimd(uint8_t *pixels, int &pos,
        int width, int x, double ci) {
        __m256d cr_vec = getCenterReal(width, x);
        __m256d ci_vec = _mm256_set1_pd(ci);

        if (isInverse) {
            __m256d cr2 = _mm256_mul_pd(cr_vec, cr_vec);
            __m256d ci2 = _mm256_mul_pd(ci_vec, ci_vec);
            __m256d cmag = _mm256_add_pd(cr2, ci2);

            __m256d mask = _mm256_cmp_pd(cmag, d_zero, _CMP_NEQ_OQ);

            cmag = _mm256_blendv_pd(d_one, cmag, mask);
            __m256d inv_cmag = _mm256_div_pd(d_one, cmag);

            cr_vec = _mm256_mul_pd(cr_vec, inv_cmag);
            ci_vec = _mm256_mul_pd(ci_vec, _mm256_mul_pd(inv_cmag, d_neg_one));
        }

        __m256d zr, zi;
        __m256d dr = d_one, di = d_zero;

        if (isJuliaSet) {
            zr = cr_vec;
            zi = ci_vec;

            cr_vec = d_seed_r_vec;
            ci_vec = d_seed_i_vec;
        } else {
            zr = d_seed_r_vec;
            zi = d_seed_i_vec;
        }

        __m256d iter = d_zero;
        __m256d mag = d_zero;

        __m256d active = _mm256_castsi256_pd(i_neg_one);

        for (int i = 0; i < ScalarGlobals::count; i++) {
            __m256d zr2 = _mm256_mul_pd(zr, zr);
            __m256d zi2 = _mm256_mul_pd(zi, zi);
            mag = _mm256_add_pd(zr2, zi2);

            active = _mm256_and_pd(active, _mm256_cmp_pd(mag, d_bailout_vec, _CMP_LT_OQ));
            if (_mm256_movemask_pd(active) == 0) break;

            switch (ScalarGlobals::colorMethod) {
                case 1:
                {
                    __m256d t1 = _mm256_sub_pd(_mm256_mul_pd(zr, dr), _mm256_mul_pd(zi, di));
                    __m256d t2 = _mm256_add_pd(_mm256_mul_pd(zr, di), _mm256_mul_pd(zi, dr));

                    __m256d new_dr = _mm256_add_pd(_mm256_add_pd(t1, t1), d_one);
                    __m256d new_di = _mm256_add_pd(t2, t2);

                    dr = _mm256_blendv_pd(dr, new_dr, active);
                    di = _mm256_blendv_pd(di, new_di, active);
                }
                break;
            }

            __m256d zrzi = _mm256_mul_pd(zr, zi);
            __m256d new_zi = _mm256_add_pd(_mm256_add_pd(zrzi, zrzi), ci_vec);
            __m256d new_zr = _mm256_add_pd(_mm256_sub_pd(zr2, zi2), cr_vec);

            iter = _mm256_add_pd(iter, _mm256_and_pd(active, d_one));
            zr = _mm256_blendv_pd(zr, new_zr, active);
            zi = _mm256_blendv_pd(zi, new_zi, active);
        }

        __m128 f_active = _mm256_cvtpd_ps(active);

        __m128 r_vec, g_vec, b_vec;
        r_vec = g_vec = b_vec = f_zero;

        switch (ScalarGlobals::colorMethod) {
            case 0:
            {
                __m128 f_iter = _mm256_cvtpd_ps(iter);
                __m128 f_mag = _mm256_cvtpd_ps(mag);

                __m128 vals = getSmoothIterVal_vec(f_iter, f_mag);
                getColorPixel_vec(vals, r_vec, g_vec, b_vec);
            }
            break;

            case 1:
            {
                __m128 f_zr = _mm256_cvtpd_ps(zr);
                __m128 f_zi = _mm256_cvtpd_ps(zi);
                __m128 f_dr = _mm256_cvtpd_ps(dr);
                __m128 f_di = _mm256_cvtpd_ps(di);

                __m128 vals = getLightVal_vec(f_zr, f_zi, f_dr, f_di);
                r_vec = g_vec = b_vec = vals;
            }
            break;
        }

        setPixelsMasked_vec(pixels, pos, width, f_active, r_vec, g_vec, b_vec);
    }
}