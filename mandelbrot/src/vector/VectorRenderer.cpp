#ifdef USE_VECTORS
#include "CommonDefs.h"
#include "VectorRenderer.h"

#include <cstdint>
#include <type_traits>

#include "../scalar/ScalarTypes.h"
#include "VectorTypes.h"

#include "../scalar/ScalarGlobals.h"
#include "VectorGlobals.h"
using namespace ScalarGlobals;
using namespace VectorGlobals;

#include "../image/Image.h"
#include "VectorCoords.h"

#define FORMULA_VECTOR
#include "../formula/FractalFormulas.h"

#include "../util/InlineUtil.h"
#include "../util/AssertUtil.h"

FORCE_INLINE void complexInverse_vec(
    simd_full_t &real, simd_full_t &imag
) {
    const simd_full_t mag = SIMD_ADDSQ_F(real, imag);
    const simd_full_mask_t mask = SIMD_ISNOT0_F(mag);

    simd_full_t invMag = SIMD_RECIP_F(mag);
    invMag = SIMD_BLEND_F(SIMD_ONE_F, invMag, mask);

    real = SIMD_MUL_F(real, invMag);
    imag = SIMD_SUBMUL_F(imag, invMag, SIMD_ZERO_F);
}

FORCE_INLINE void _initCoords_vec(
    simd_full_t &cr, simd_full_t &ci,
    simd_full_t &zr, simd_full_t &zi,
    simd_full_t &dr, simd_full_t &di
) {
    if (isInverse) complexInverse_vec(cr, ci);

    if (isJuliaSet) {
        zr = cr;
        zi = ci;

        cr = f_seed_r_vec;
        ci = f_seed_i_vec;
    } else if (normalSeed) {
        zr = cr;
        zi = ci;
    } else {
        zr = f_seed_r_vec;
        zi = f_seed_i_vec;
    }

    dr = SIMD_ONE_F;
    di = SIMD_ZERO_F;
}

FORCE_INLINE simd_full_t _iterateFractal_vec(
    simd_full_t cr, simd_full_t ci,
    simd_full_t &zr, simd_full_t &zi,
    simd_full_t &dr, simd_full_t &di,
    simd_full_t &outMag, simd_full_mask_t &outActive
) {
    simd_full_t iter = startIterVal;
    simd_full_t mag = SIMD_ZERO_F;

    simd_full_mask_t active = SIMD_INIT_ONES_MASK_F;

    for (int i = 0; i < count; i++) {
        mag = SIMD_ADDSQ_F(zr, zi);

        const simd_full_mask_t itering = SIMD_CMP_LT_F(mag, f_bailout_vec);
        active = SIMD_AND_MASK_F(active, itering);

        if (SIMD_MASK_ALLZERO_F(active)) break;

        switch (colorMethod) {
            case 2:
            {
                simd_full_t new_dr, new_di;
                derivative(
                    zr, zi,
                    dr, di,
                    mag,
                    new_dr, new_di
                );

                dr = SIMD_BLEND_F(dr, new_dr, active);
                di = SIMD_BLEND_F(di, new_di, active);
            }
            break;

            default:
                break;
        }

        simd_full_t new_zr, new_zi;
        formula(
            cr, ci,
            zr, zi,
            mag,
            new_zr, new_zi
        );

        iter = SIMD_ADD_MASK_F(iter, SIMD_ONE_F, active);
        zr = SIMD_BLEND_F(zr, new_zr, active);
        zi = SIMD_BLEND_F(zi, new_zi, active);
    }

    outMag = mag;
    outActive = active;

    return iter;
}

#ifdef USE_VECTOR_STORE

#include <array>

#include <emmintrin.h>
#include <tmmintrin.h>

constexpr int WIDTH_128_STRIDE = 4 * Image::STRIDE;

template <int N, int total>
consteval auto _makeRgbMask() {
    std::array<int8_t, total> mask{};

    for (int i = 0; i < N; i++) {
        mask[i * 3] = CAST_INT_S(i, 8);
        mask[i * 3 + 1] = CAST_INT_S(i + 4, 8);
        mask[i * 3 + 2] = CAST_INT_S(i + 8, 8);
    }

    for (int i = N * 3; i < total; i++) mask[i] = -1;
    return mask;
}

constexpr auto _rgbMask_arr = _makeRgbMask<4, 16>();
static_assert(is_constexpr_test<_rgbMask_arr>());

static const __m128i _rgbMask = _mm_loadu_si128(
    reinterpret_cast<const __m128i *>(_rgbMask_arr.data())
);

FORCE_INLINE void store128bitLane_vec(
    uint8_t *out,
    __m128i data, int lane
) {
    uint8_t *ptr = out + lane * WIDTH_128_STRIDE;
    _mm_storeu_si128(reinterpret_cast<__m128i *>(ptr), data);
}

template<typename T>
FORCE_INLINE void writePixelData_vec(uint8_t *out, T RGBA8) {
    if constexpr (std::is_same_v<T, __m128i>) {
        const __m128i mix = _mm_shuffle_epi8(RGBA8, _rgbMask);
        store128bitLane_vec(out, mix, 0);
    } else if constexpr (std::is_same_v<T, __m256i>) {
        const __m128i lanes[2] = {
            _mm256_castsi256_si128(RGBA8),
            _mm256_extracti128_si256(RGBA8, 1)
        };

        FULL_UNROLL;
        for (int i = 0; i < 2; i++) {
            const __m128i mix = _mm_shuffle_epi8(lanes[i], _rgbMask);
            store128bitLane_vec(out, mix, i);
        }
    } else if constexpr (std::is_same_v<T, __m512i>) {
        const __m128i lanes[4] = {
            _mm512_extracti32x4_epi32(RGBA8, 0),
            _mm512_extracti32x4_epi32(RGBA8, 1),
            _mm512_extracti32x4_epi32(RGBA8, 2),
            _mm512_extracti32x4_epi32(RGBA8, 3),
        };

        FULL_UNROLL;
        for (int i = 0; i < 4; i++) {
            const __m128i mix = _mm_shuffle_epi8(lanes[i], _rgbMask);
            store128bitLane_vec(out, mix, i);
        }
    } else {
        STATIC_FAIL_MSG(T, "Unsupported SIMD type");
    }
}

#else
#include "../scalar/ScalarRenderer.h"
#endif

FORCE_INLINE simd_half_t normCos_vec(simd_half_t x) {
    const simd_half_t a = SIMD_COS_H(x);
    return SIMD_MULADD_H(a, SIMD_ONEHALF_H, SIMD_ONEHALF_H);
}

FORCE_INLINE void getPixelColor_vec(
    simd_half_t val,
    simd_half_t &outR, simd_half_t &outG, simd_half_t &outB
) {
#if false
    const simd_half_t R_x = SIMD_MULADD_H(
        val, h_freq_r_vec,
        h_phase_r_vec
    );

    const simd_half_t G_x = SIMD_MULADD_H(
        val, h_freq_g_vec,
        h_phase_g_vec
    );

    const simd_half_t B_x = SIMD_MULADD_H(
        val, h_freq_b_vec,
        h_phase_b_vec
    );

    outR = normCos_vec(R_x);
    outG = normCos_vec(G_x);
    outB = normCos_vec(B_x);
#else
    palette_vec.sampleSIMD(val, outR, outG, outB);
#endif
}

FORCE_INLINE simd_half_t getIterVal_vec(simd_half_t iter) {
#ifdef NORM_ITER_COUNT
    return SIMD_MUL_H(iter, h_invCount_vec);
#else
    return iter;
#endif
}

FORCE_INLINE simd_half_t getSmoothIterVal_vec(
    simd_half_t iter, simd_half_t mag
) {
    const simd_half_t lg1 = SIMD_LOG_H(SIMD_SQRT_H(mag));
    const simd_half_t lg2 = SIMD_LOG_H(SIMD_MUL_H(lg1, h_invLnBail_vec));
    return SIMD_SUBMUL_H(lg2, h_invLnPow_vec, iter);
}

FORCE_INLINE simd_half_t getLightVal_vec(
    simd_half_t zr, simd_half_t zi,
    simd_half_t dr, simd_half_t di
) {
    const simd_half_t dinv = SIMD_RECIP_H(SIMD_ADDSQ_H(dr, di));

    simd_half_t ur = SIMD_MUL_H(
        SIMD_ADDXX_H(zr, dr, zi, di),
        dinv
    );

    simd_half_t ui = SIMD_MUL_H(
        SIMD_SUBXX_H(zi, dr, zr, di),
        dinv
    );

    const simd_half_t umag = SIMD_RSQRT_H(SIMD_ADDSQ_H(ur, ui));
    ur = SIMD_MUL_H(ur, umag);
    ui = SIMD_MUL_H(ui, umag);

    const simd_half_t val = SIMD_ADD_H(
        SIMD_SUBXX_H(
            ur, h_light_r_vec,
            ui, h_light_i_vec
        ),
        h_light_h_vec
    );

    const simd_half_t light = SIMD_DIV_H(
        val,
        SIMD_ADD_H(h_light_h_vec, SIMD_ONE_H)
    );

#ifdef NORM_ITER_COUNT
    return SIMD_MUL_H(
        SIMD_MAX_H(light, SIMD_ZERO_H),
        h_invCount_vec
    );
#else
    return SIMD_MAX_H(light, SIMD_ZERO_H);
#endif
}

FORCE_INLINE simd_half_int_t _colorToInt_vec(simd_half_t val) {
    simd_half_t newVal = SIMD_MUL_H(val, SIMD_CONSTSET_H(255.0));
    newVal = SIMD_CLAMP_H(newVal, SIMD_ZERO_H, SIMD_CONSTSET_H(255.0));
    return SIMD_HALF_TO_INT_CONV(newVal);
}

FORCE_INLINE void _setPixels_vec(
    uint8_t *pixels, size_t &pos, int width,
    simd_half_t R, simd_half_t G, simd_half_t B
) {
    const simd_half_t colorSum = SIMD_ADD_H(R, SIMD_ADD_H(G, B));
    const simd_half_mask_t underThresh =
        SIMD_CMP_LT_H(colorSum, h_colorEps_vec);

    const int byteCount = width * Image::STRIDE;

    if (SIMD_MASK_ALLONES_H(underThresh)) {
        pos += byteCount;
        return;
    }

    uint8_t *out = pixels + pos;

#ifdef USE_VECTOR_STORE
    const simd_half_int_t R_i = _colorToInt_vec(R);
    const simd_half_int_t G_i = _colorToInt_vec(G);
    const simd_half_int_t B_i = _colorToInt_vec(B);

    const simd_half_int_t RG16 = SIMD_PACK_USAT_INT32_H(R_i, G_i);
    const simd_half_int_t BZ16 = SIMD_PACK_USAT_INT32_H(B_i, SIMD_ZERO_HI);
    const simd_half_int_t RGBA8 = SIMD_PACK_USAT_INT16_H(RG16, BZ16);

    writePixelData_vec(out, RGBA8);
#else
    alignas(SIMD_HALF_ALIGNMENT) scalar_half_t R_arr[SIMD_HALF_WIDTH];
    alignas(SIMD_HALF_ALIGNMENT) scalar_half_t G_arr[SIMD_HALF_WIDTH];
    alignas(SIMD_HALF_ALIGNMENT) scalar_half_t B_arr[SIMD_HALF_WIDTH];

    SIMD_STORE_H(R_arr, R);
    SIMD_STORE_H(G_arr, G);
    SIMD_STORE_H(B_arr, B);

    UNROLL(SIMD_HALF_WIDTH);
    for (size_t i = 0; i < width; i++) {
        out[i * Image::STRIDE] = ScalarRenderer::colorToInt(R_arr[i]);
        out[i * Image::STRIDE + 1] = ScalarRenderer::colorToInt(G_arr[i]);
        out[i * Image::STRIDE + 2] = ScalarRenderer::colorToInt(B_arr[i]);
    }
#endif

    pos += byteCount;
}

FORCE_INLINE void setPixelsMasked_vec(
    uint8_t *pixels, size_t &pos, int width,
    simd_half_t active,
    simd_half_t R, simd_half_t G, simd_half_t B
) {
    const simd_half_mask_t inactive = SIMD_IS0_H(active);

    const simd_half_t R_m = SIMD_MASK_AND_H(R, inactive);
    const simd_half_t G_m = SIMD_MASK_AND_H(G, inactive);
    const simd_half_t B_m = SIMD_MASK_AND_H(B, inactive);

    _setPixels_vec(pixels, pos, width, R_m, G_m, B_m);
}

FORCE_INLINE void _colorPixels_vec(
    uint8_t *pixels, size_t &pos, int width,
    simd_full_t iter, simd_full_t mag,
    simd_full_mask_t active,
    simd_full_t zr, simd_full_t zi,
    simd_full_t dr, simd_full_t di
) {
    const simd_half_t h_active = SIMD_FULL_MASK_TO_HALF(active);

    simd_half_t vals, r_vec, g_vec, b_vec;
    r_vec = g_vec = b_vec = SIMD_ZERO_H;

    switch (colorMethod) {
        case 0:
        {
            const simd_half_t h_iter = SIMD_FULL_TO_HALF_CONV(iter);

            vals = getIterVal_vec(h_iter);
            getPixelColor_vec(vals, r_vec, g_vec, b_vec);
        }
        break;

        case 1:
        {
            const simd_half_t h_iter = SIMD_FULL_TO_HALF_CONV(iter);
            const simd_half_t h_mag = SIMD_FULL_TO_HALF_CONV(mag);

            vals = getSmoothIterVal_vec(h_iter, h_mag);
            getPixelColor_vec(vals, r_vec, g_vec, b_vec);
        }
        break;

        case 2:
        {
            const simd_half_t h_zr = SIMD_FULL_TO_HALF_CONV(zr);
            const simd_half_t h_zi = SIMD_FULL_TO_HALF_CONV(zi);
            const simd_half_t h_dr = SIMD_FULL_TO_HALF_CONV(dr);
            const simd_half_t h_di = SIMD_FULL_TO_HALF_CONV(di);

            vals = getLightVal_vec(h_zr, h_zi, h_dr, h_di);
            r_vec = g_vec = b_vec = vals;
        }
        break;

        default:
            break;
    }

    setPixelsMasked_vec(
        pixels, pos, width,
        h_active,
        r_vec, g_vec, b_vec
    );
}

namespace VectorRenderer {
    void VECTOR_CALL initCoordsSIMD(
        simd_full_t &cr, simd_full_t &ci,
        simd_full_t &zr, simd_full_t &zi,
        simd_full_t &dr, simd_full_t &di
    ) {
        _initCoords_vec(
            cr, ci,
            zr, zi,
            dr, di
        );
    }

    simd_full_t VECTOR_CALL iterateFractalSIMD(
        simd_full_t cr, simd_full_t ci,
        simd_full_t &zr, simd_full_t &zi,
        simd_full_t &dr, simd_full_t &di,
        simd_full_t &mag, simd_full_mask_t &active
    ) {
        return _iterateFractal_vec(
            cr, ci,
            zr, zi,
            dr, di,
            mag, active
        );
    }

    simd_half_int_t VECTOR_CALL colorToIntSIMD(simd_half_t val) {
        return _colorToInt_vec(val);
    }

    void VECTOR_CALL setPixelsSIMD(
        uint8_t *pixels, size_t &pos, int width,
        simd_half_t R, simd_half_t G, simd_half_t B
    ) {
        _setPixels_vec(
            pixels, pos, width,
            R, G, B
        );
    }

    void VECTOR_CALL colorPixelsSIMD(
        uint8_t *pixels, size_t &pos, int width,
        simd_full_t iter, simd_full_t mag,
        simd_full_mask_t active,
        simd_full_t zr, simd_full_t zi,
        simd_full_t dr, simd_full_t di
    ) {
        _colorPixels_vec(
            pixels, pos, width,
            iter, mag,
            active,
            zr, zi,
            dr, di
        );
    }

    void VECTOR_CALL renderPixelSIMD(
        uint8_t *pixels, size_t &pos, int width,
        scalar_full_t x, simd_full_t ci, uint64_t *totalIterCount
    ) {
        simd_full_t cr = getRealPoints_vec(width, x);

        simd_full_t zr, zi;
        simd_full_t dr, di;
        _initCoords_vec(
            cr, ci,
            zr, zi,
            dr, di
        );

        simd_full_t mag;
        simd_full_mask_t active;
        const simd_full_t iter = _iterateFractal_vec(
            cr, ci,
            zr, zi,
            dr, di,
            mag, active
        );

        _colorPixels_vec(
            pixels, pos, width,
            iter, mag,
            active,
            zr, zi,
            dr, di
        );

        if (totalIterCount) {
            const scalar_full_t sum = SIMD_HORIZ_ADD_F(iter);
            *totalIterCount = static_cast<uint64_t>(sum);
        }
    }
}

#endif