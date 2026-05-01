#ifdef USE_VECTORS
#include "CommonDefs.h"
#include "VectorRenderer.h"

#include <cstdint>
#include <type_traits>
#include <algorithm>

#include "scalar/ScalarTypes.h"
#include "VectorTypes.h"

#include "scalar/ScalarGlobals.h"
#include "VectorGlobals.h"
using namespace ScalarGlobals;
using namespace VectorGlobals;

#include "image/Image.h"
#include "render/RenderIterationStats.h"
#include "VectorCoords.h"

#define _FORMULA_VECTOR
#define _SKIP_FORMULA_OPS
#include "formula/FormulaTypes.h"

#include "util/InlineUtil.h"
#include "util/AssertUtil.h"

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

#define _INVALID_VAR(sym, n) _CONCAT2(sym, n)

#define _INVALID_POWER(n) \
    _INVALID_VAR(zr, n) = _INVALID_VAR(cr, n); \
    _INVALID_VAR(zi, n) = _INVALID_VAR(ci, n); \
    _INVALID_VAR(dr, n) = _INVALID_VAR(di, n) = SIMD_ZERO_F; \
    _INVALID_VAR(mag, n) = SIMD_ADDXX_F( \
        _INVALID_VAR(zr, n), _INVALID_VAR(zr, n), \
        _INVALID_VAR(zi, n), _INVALID_VAR(zi, n) \
    ); \
    _INVALID_VAR(iter, n) = SIMD_BLEND_F( \
        _INVALID_VAR(iter, n), SIMD_SET1_F(count), \
        SIMD_CMP_LT_F(_INVALID_VAR(mag, n), f_bailout_vec) \
    ); \
    _INVALID_VAR(active, n) = SIMD_INIT_ZERO_MASK_F;

FORCE_INLINE simd_full_t _iterateFractal_vec(
    simd_full_t cr, simd_full_t ci,
    simd_full_t &zr, simd_full_t &zi,
    simd_full_t &dr, simd_full_t &di,
    simd_full_t &outMag, simd_full_mask_t &outActive
) {
    simd_full_t iter = startIterVal;
    simd_full_mask_t active = SIMD_INIT_ONES_MASK_F;

    simd_full_t zr2, zi2;
    simd_full_t mag = SIMD_ZERO_F;

    simd_full_t new_zr = zr, new_zi = zi;
    simd_full_t new_dr = dr, new_di = di;

    if (invalidPower) {
        simd_full_t &cr1 = cr, &ci1 = ci;
        simd_full_t &zr1 = zr, &zi1 = zi;
        simd_full_t &dr1 = dr, &di1 = di;
        simd_full_t &iter1 = iter, &mag1 = mag;
        simd_full_mask_t &active1 = active;

        _INVALID_POWER(1);
    } else {
        switch (fractalType) {
            case 0:
#define _FRACTAL_TYPE mandelbrot
#include "loop/FractalLoop.h"
#undef _FRACTAL_TYPE
                break;

            case 1:
#define _FRACTAL_TYPE perpendicular
#include "loop/FractalLoop.h"
#undef _FRACTAL_TYPE
                break;

            case 2:
#define _FRACTAL_TYPE burningship
#include "loop/FractalLoop.h"
#undef _FRACTAL_TYPE
                break;

            default:
                break;
        }
    }

    outMag = mag;
    outActive = active;
    return iter;
}

#define _QUAD_OUTPUT(n) \
    _INVALID_VAR(outIter, n) = _INVALID_VAR(iter, n); \
    _INVALID_VAR(outMag, n) = _INVALID_VAR(mag, n); \
    _INVALID_VAR(outActive, n) = _INVALID_VAR(active, n);

FORCE_INLINE void _iterateFractal4_vec(
    simd_full_t cr1, simd_full_t ci1,
    simd_full_t &zr1, simd_full_t &zi1,
    simd_full_t &dr1, simd_full_t &di1,
    simd_full_t &outIter1, simd_full_t &outMag1, simd_full_mask_t &outActive1,
    simd_full_t cr2, simd_full_t ci2,
    simd_full_t &zr2, simd_full_t &zi2,
    simd_full_t &dr2, simd_full_t &di2,
    simd_full_t &outIter2, simd_full_t &outMag2, simd_full_mask_t &outActive2,
    simd_full_t cr3, simd_full_t ci3,
    simd_full_t &zr3, simd_full_t &zi3,
    simd_full_t &dr3, simd_full_t &di3,
    simd_full_t &outIter3, simd_full_t &outMag3, simd_full_mask_t &outActive3,
    simd_full_t cr4, simd_full_t ci4,
    simd_full_t &zr4, simd_full_t &zi4,
    simd_full_t &dr4, simd_full_t &di4,
    simd_full_t &outIter4, simd_full_t &outMag4, simd_full_mask_t &outActive4
) {
    simd_full_t iter1 = startIterVal;
    simd_full_t iter2 = startIterVal;
    simd_full_t iter3 = startIterVal;
    simd_full_t iter4 = startIterVal;

    simd_full_mask_t active1 = outActive1;
    simd_full_mask_t active2 = outActive2;
    simd_full_mask_t active3 = outActive3;
    simd_full_mask_t active4 = outActive4;

    simd_full_t zr21, zi21;
    simd_full_t zr22, zi22;
    simd_full_t zr23, zi23;
    simd_full_t zr24, zi24;
    simd_full_t mag1 = SIMD_ZERO_F;
    simd_full_t mag2 = SIMD_ZERO_F;
    simd_full_t mag3 = SIMD_ZERO_F;
    simd_full_t mag4 = SIMD_ZERO_F;

    simd_full_t new_zr1 = zr1, new_zi1 = zi1;
    simd_full_t new_dr1 = dr1, new_di1 = di1;
    simd_full_t new_zr2 = zr2, new_zi2 = zi2;
    simd_full_t new_dr2 = dr2, new_di2 = di2;
    simd_full_t new_zr3 = zr3, new_zi3 = zi3;
    simd_full_t new_dr3 = dr3, new_di3 = di3;
    simd_full_t new_zr4 = zr4, new_zi4 = zi4;
    simd_full_t new_dr4 = dr4, new_di4 = di4;

    if (invalidPower) {
        _INVALID_POWER(1);
        _INVALID_POWER(2);
        _INVALID_POWER(3);
        _INVALID_POWER(4);
    } else {
        switch (fractalType) {
            case 0:
#define _FRACTAL_TYPE mandelbrot
#include "loop/FractalLoop4.h"
#undef _FRACTAL_TYPE
                break;

            default:
                break;
        }
    }

    _QUAD_OUTPUT(1);
    _QUAD_OUTPUT(2);
    _QUAD_OUTPUT(3);
    _QUAD_OUTPUT(4);
}

#ifdef USE_VECTOR_STORE

#include <array>

#include <emmintrin.h>
#include <tmmintrin.h>

constexpr int WIDTH_128_STRIDE = 4 * Image::STRIDE;

template <int N, int total>
consteval auto _makeRGBMask() {
    std::array<int8_t, total> mask{};

    for (int i = 0; i < N; i++) {
        mask[i * 3] = CAST_INT_S(i, 8);
        mask[i * 3 + 1] = CAST_INT_S(i + 4, 8);
        mask[i * 3 + 2] = CAST_INT_S(i + 8, 8);
    }

    for (int i = N * 3; i < total; i++) mask[i] = -1;
    return mask;
}

constexpr auto _rgbMask_arr = _makeRGBMask<4, 16>();
static_assert(is_constexpr_test<_rgbMask_arr>());

static const __m128i _rgbMask = _mm_loadu_si128(
    reinterpret_cast<const __m128i *>(_rgbMask_arr.data())
);

#define shuffleRGB_vec(rgb) _mm_shuffle_epi8(rgb, _rgbMask)

#define store128bitLane_vec(out, data, lane) \
    _mm_storeu_si128(reinterpret_cast<__m128i *>( \
        out + lane * WIDTH_128_STRIDE), data);

template<typename T>
FORCE_INLINE void writePixelData_vec(uint8_t *out, T RGBA8) {
    if constexpr (std::is_same_v<T, __m128i>) {
        store128bitLane_vec(out, shuffleRGB_vec(RGBA8), 0);
    } else if constexpr (std::is_same_v<T, __m256i>) {
        store128bitLane_vec(out, shuffleRGB_vec(
            _mm256_castsi256_si128(RGBA8)), 0);
        store128bitLane_vec(out, shuffleRGB_vec(
            _mm256_extracti128_si256(RGBA8, 1)), 1);
    } else if constexpr (std::is_same_v<T, __m512i>) {
        store128bitLane_vec(out, shuffleRGB_vec(
            _mm512_extracti32x4_epi32(RGBA8, 0)), 0);
        store128bitLane_vec(out, shuffleRGB_vec(
            _mm512_extracti32x4_epi32(RGBA8, 1)), 1);
        store128bitLane_vec(out, shuffleRGB_vec(
            _mm512_extracti32x4_epi32(RGBA8, 2)), 2);
        store128bitLane_vec(out, shuffleRGB_vec(
            _mm512_extracti32x4_epi32(RGBA8, 3)), 3);
    } else STATIC_FAIL_MSG(T, "Unsupported SIMD type");
}

#else
#include "scalar/ScalarColors.h"
#endif

#define normCos_vec(x) \
    SIMD_MULADD_H(SIMD_COS_H(x), SIMD_ONEHALF_H, SIMD_ONEHALF_H)

FORCE_INLINE void getPixelColor_vec(
    simd_half_t val,
    simd_half_t &outR, simd_half_t &outG, simd_half_t &outB
) {
    outR = normCos_vec(SIMD_MULADD_H(
        val, sinePalette_vec.getFreqR(),
        sinePalette_vec.getPhaseR()
    ));
    outG = normCos_vec(SIMD_MULADD_H(
        val, sinePalette_vec.getFreqG(),
        sinePalette_vec.getPhaseG()
    ));
    outB = normCos_vec(SIMD_MULADD_H(
        val, sinePalette_vec.getFreqB(),
        sinePalette_vec.getPhaseB()
    ));
}

FORCE_INLINE void getPaletteColor_vec(
    simd_half_t val,
    simd_half_t &outR, simd_half_t &outG, simd_half_t &outB
) {
    palette_vec.sampleSIMD(val, outR, outG, outB);
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
    simd_half_t val;

    switch (fractalType) {
        case 0:
        {
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

            val = SIMD_ADD_H(
                SIMD_SUBXX_H(
                    ur, h_light_r_vec,
                    ui, h_light_i_vec
                ),
                h_light_h_vec
            );
            break;
        }
        default:
        {
            const simd_half_t zmag = SIMD_RSQRT_H(SIMD_ADDSQ_H(zr, zi));
            const simd_half_t dmag = SIMD_RSQRT_H(SIMD_ADDSQ_H(dr, di));

            const simd_half_t s = SIMD_MUL_H(
                SIMD_ADDXX_H(zr, dr, zi, di),
                SIMD_MUL_H(zmag, dmag)
            );

            val = SIMD_ADD_H(s, h_light_h_vec);
            break;
        }
    }

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

#define _colorToInt_vec(val) \
    SIMD_HALF_TO_INT_CONV(SIMD_CLAMP_H( \
        SIMD_MUL_H(val, SIMD_CONSTSET_H(255.0)), \
        SIMD_ZERO_H, SIMD_CONSTSET_H(255.0) \
    ))

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
    const simd_half_int_t RG16 = SIMD_PACK_USAT_INT32_H(
        _colorToInt_vec(R),
        _colorToInt_vec(G)
    );
    const simd_half_int_t BZ16 = SIMD_PACK_USAT_INT32_H(
        _colorToInt_vec(B),
        SIMD_ZERO_HI
    );

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
        out[i * Image::STRIDE] = toColorByte(R_arr[i]);
        out[i * Image::STRIDE + 1] = toColorByte(G_arr[i]);
        out[i * Image::STRIDE + 2] = toColorByte(B_arr[i]);
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
    simd_half_t vals, r_vec, g_vec, b_vec;
    r_vec = g_vec = b_vec = SIMD_ZERO_H;

    switch (colorMethod) {
        case 0:
        {
            vals = getIterVal_vec(SIMD_FULL_TO_HALF_CONV(iter));
            getPixelColor_vec(vals, r_vec, g_vec, b_vec);
        }
        break;

        case 1:
        {
            vals = getSmoothIterVal_vec(
                SIMD_FULL_TO_HALF_CONV(iter),
                SIMD_FULL_TO_HALF_CONV(mag)
            );
            getPixelColor_vec(vals, r_vec, g_vec, b_vec);
        }
        break;

        case 2:
        {
            vals = getSmoothIterVal_vec(
                SIMD_FULL_TO_HALF_CONV(iter),
                SIMD_FULL_TO_HALF_CONV(mag)
            );
            getPaletteColor_vec(vals, r_vec, g_vec, b_vec);
        }
        break;

        case 3:
        {
            vals = getLightVal_vec(
                SIMD_FULL_TO_HALF_CONV(zr),
                SIMD_FULL_TO_HALF_CONV(zi),
                SIMD_FULL_TO_HALF_CONV(dr),
                SIMD_FULL_TO_HALF_CONV(di)
            );

            r_vec = SIMD_MUL_H(vals, h_lightColor_r_vec);
            g_vec = SIMD_MUL_H(vals, h_lightColor_g_vec);
            b_vec = SIMD_MUL_H(vals, h_lightColor_b_vec);
        }
        break;

        default:
            break;
    }

    setPixelsMasked_vec(
        pixels, pos, width,
        SIMD_FULL_MASK_TO_HALF(active),
        r_vec, g_vec, b_vec
    );
}

#define _RENDER_VAR(sym, n) _CONCAT2(sym, n)

#define _RENDER_OFFSET(n) ((n) - 1) * SIMD_FULL_WIDTH

#define _RENDER_WIDTH(n) \
    const int _RENDER_VAR(width, n) = width > _RENDER_OFFSET(n) \
        ? ((width - _RENDER_OFFSET(n)) < SIMD_FULL_WIDTH \
            ? (width - _RENDER_OFFSET(n)) \
            : SIMD_FULL_WIDTH) \
        : 0;

#define _RENDER_COORD(n) \
    simd_full_t _RENDER_VAR(cr, n) = SIMD_ZERO_F; \
    if (_RENDER_VAR(width, n) > 0) { \
        _RENDER_VAR(cr, n) = getRealPoints_vec( \
            _RENDER_VAR(width, n), x + _RENDER_OFFSET(n) \
        ); \
    }

#define _RENDER_STATE(n) \
    simd_full_t _RENDER_VAR(zr, n) = SIMD_ZERO_F; \
    simd_full_t _RENDER_VAR(zi, n) = SIMD_ZERO_F; \
    simd_full_t _RENDER_VAR(dr, n) = SIMD_ONE_F; \
    simd_full_t _RENDER_VAR(di, n) = SIMD_ZERO_F; \
    simd_full_t _RENDER_VAR(iter, n) = SIMD_ZERO_F; \
    simd_full_t _RENDER_VAR(mag, n) = SIMD_ZERO_F; \
    simd_full_mask_t _RENDER_VAR(active, n) = SIMD_INIT_ZERO_MASK_F;

#define _RENDER_INIT(n) \
    if (_RENDER_VAR(width, n) > 0) { \
        _initCoords_vec( \
            _RENDER_VAR(cr, n), ci, \
            _RENDER_VAR(zr, n), _RENDER_VAR(zi, n), \
            _RENDER_VAR(dr, n), _RENDER_VAR(di, n) \
        ); \
        _RENDER_VAR(active, n) = SIMD_INIT_ONES_MASK_F; \
    }

#define _RENDER_COLOR(n) \
    if (_RENDER_VAR(width, n) > 0) { \
        _colorPixels_vec( \
            pixels, pos, _RENDER_VAR(width, n), \
            _RENDER_VAR(iter, n), _RENDER_VAR(mag, n), \
            _RENDER_VAR(active, n), \
            _RENDER_VAR(zr, n), _RENDER_VAR(zi, n), \
            _RENDER_VAR(dr, n), _RENDER_VAR(di, n) \
        ); \
    }

FORCE_INLINE void recordIterationStats_vec(
    OptionalIterationStats iterStats,
    simd_full_t iter,
    int validWidth,
    int x,
    std::optional<int> y
) {
    if (!iterStats) return;

    alignas(SIMD_FULL_ALIGNMENT) scalar_full_t iterLanes[SIMD_FULL_WIDTH];
    SIMD_STORE_F(iterLanes, iter);

    for (int lane = 0; lane < validWidth; lane++) {
        const int64_t pixelIter = static_cast<int64_t>(
            std::min<scalar_full_t>(
                iterLanes[lane],
                static_cast<scalar_full_t>(count)));
        iterStats->get().record(pixelIter, x + lane, y);
    }
}

#define _RENDER_ITER_STATS(n) \
    recordIterationStats_vec( \
        iterStats, _RENDER_VAR(iter, n), _RENDER_VAR(width, n), \
        static_cast<int>(x) + _RENDER_OFFSET(n), y)

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
        scalar_full_t x, simd_full_t ci,
        OptionalIterationStats iterStats, std::optional<int> y
    ) {
        if (!useQuadPath()) {
            _RENDER_WIDTH(1);
            _RENDER_COORD(1);
            _RENDER_STATE(1);
            _RENDER_INIT(1);

            if (width1 > 0) {
                iter1 = _iterateFractal_vec(
                    cr1, ci,
                    zr1, zi1,
                    dr1, di1,
                    mag1, active1
                );
            }

            _RENDER_COLOR(1);

            _RENDER_ITER_STATS(1);
        } else {
            _RENDER_WIDTH(1);
            _RENDER_WIDTH(2);
            _RENDER_WIDTH(3);
            _RENDER_WIDTH(4);

            _RENDER_COORD(1);
            _RENDER_COORD(2);
            _RENDER_COORD(3);
            _RENDER_COORD(4);

            _RENDER_STATE(1);
            _RENDER_STATE(2);
            _RENDER_STATE(3);
            _RENDER_STATE(4);

            _RENDER_INIT(1);
            _RENDER_INIT(2);
            _RENDER_INIT(3);
            _RENDER_INIT(4);

            _iterateFractal4_vec(
                cr1, ci,
                zr1, zi1,
                dr1, di1,
                iter1, mag1, active1,
                cr2, ci,
                zr2, zi2,
                dr2, di2,
                iter2, mag2, active2,
                cr3, ci,
                zr3, zi3,
                dr3, di3,
                iter3, mag3, active3,
                cr4, ci,
                zr4, zi4,
                dr4, di4,
                iter4, mag4, active4
            );

            _RENDER_COLOR(1);
            _RENDER_COLOR(2);
            _RENDER_COLOR(3);
            _RENDER_COLOR(4);

            _RENDER_ITER_STATS(1);
            _RENDER_ITER_STATS(2);
            _RENDER_ITER_STATS(3);
            _RENDER_ITER_STATS(4);
        }
    }
}

#endif