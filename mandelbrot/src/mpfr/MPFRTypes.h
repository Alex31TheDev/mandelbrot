#pragma once
#ifdef USE_MPFR

#include <cctype>
#include <cstddef>
#include <algorithm>
#include <cmath>
#include <string>

#include <mpfr.h>

#include "MPFRGlobals.h"
#include "ThreadLocalPool.h"
#include "util/InlineUtil.h"

namespace MPFRTypes {
    constexpr mpfr_rnd_t ROUNDING = MPFR_RNDN;
    constexpr size_t TEMP_POOL_SIZE = 128;

    FORCE_INLINE constexpr mpfr_prec_t digits2bits(int digits) {
        const int safeDigits = digits < 2 ? 2 : digits;
        return static_cast<mpfr_prec_t>(safeDigits * 4);
    }

    inline mpfr_prec_t precisionBits = digits2bits(MPFRGlobals::MIN_PRECISION_DIGITS);

    struct TempValue {
        mpfr_t value;
        bool initialized = false;

        TempValue() = default;
        TempValue(const TempValue &) = delete;
        TempValue &operator=(const TempValue &) = delete;

        ~TempValue() {
            if (initialized) {
                mpfr_clear(value);
            }
        }
    };

    FORCE_INLINE void setPrecisionBits(mpfr_prec_t bits) {
        precisionBits = bits > 1 ? bits :
            digits2bits(MPFRGlobals::MIN_PRECISION_DIGITS);
    }

    FORCE_INLINE void initRaw(mpfr_t value) {
        mpfr_init2(value, precisionBits);
    }

    FORCE_INLINE void initRaw(mpfr_t value, double initialValue) {
        initRaw(value);
        mpfr_set_d(value, initialValue, ROUNDING);
    }

    FORCE_INLINE void ensurePrecision(TempValue &slot) {
        if (!slot.initialized) {
            mpfr_init2(slot.value, precisionBits);
            slot.initialized = true;
            return;
        }

        if (mpfr_get_prec(slot.value) != precisionBits) {
            mpfr_prec_round(slot.value, precisionBits, ROUNDING);
        }
    }

    FORCE_INLINE std::string toString(mpfr_srcptr value) {
        const int digits = std::max(
            MPFRGlobals::MIN_PRECISION_DIGITS,
            static_cast<int>(std::ceil(
                static_cast<double>(mpfr_get_prec(value)) * MPFRGlobals::LOG10_2))
        ) + MPFRGlobals::STRING_GUARD_DIGITS;


        char *buffer = nullptr;
        if (mpfr_asprintf(&buffer, "%.*Rg", digits, value) < 0 || !buffer) {
            return "0";
        }

        std::string out(buffer);
        mpfr_free_str(buffer);
        return out;
    }

    FORCE_INLINE bool parseNumber(mpfr_t out, const char *text) {
        if (text == nullptr) return false;

        while (std::isspace(static_cast<unsigned char>(*text))) ++text;
        if (*text == '\0') return false;

        char *end = nullptr;
        mpfr_strtofr(out, text, &end, 10, ROUNDING);
        if (end == text) return false;

        while (*end != '\0' &&
            std::isspace(static_cast<unsigned char>(*end))) ++end;

        if (*end != '\0') return false;
        return mpfr_number_p(out) != 0 || mpfr_zero_p(out) != 0;
    }

    FORCE_INLINE bool isValidNumber(const char *text) {
        mpfr_t value;
        mpfr_init2(value, 64);
        const bool valid = parseNumber(value, text);
        mpfr_clear(value);
        return valid;
    }

    FORCE_INLINE mpfr_ptr nextTemp() {
        thread_local ThreadLocalPool<TempValue, TEMP_POOL_SIZE> pool;
        TempValue &slot = pool.getNextItem();
        ensurePrecision(slot);
        return slot.value;
    }

    FORCE_INLINE mpfr_srcptr toNumber(double value) {
        mpfr_ptr out = nextTemp();
        mpfr_set_d(out, value, ROUNDING);
        return out;
    }

    FORCE_INLINE mpfr_srcptr add(mpfr_srcptr a, mpfr_srcptr b) {
        mpfr_ptr out = nextTemp();
        mpfr_add(out, a, b, ROUNDING);
        return out;
    }

    FORCE_INLINE mpfr_srcptr sub(mpfr_srcptr a, mpfr_srcptr b) {
        mpfr_ptr out = nextTemp();
        mpfr_sub(out, a, b, ROUNDING);
        return out;
    }

    FORCE_INLINE mpfr_srcptr mul(mpfr_srcptr a, mpfr_srcptr b) {
        mpfr_ptr out = nextTemp();
        mpfr_mul(out, a, b, ROUNDING);
        return out;
    }

    FORCE_INLINE mpfr_srcptr div(mpfr_srcptr a, mpfr_srcptr b) {
        mpfr_ptr out = nextTemp();
        mpfr_div(out, a, b, ROUNDING);
        return out;
    }

    FORCE_INLINE mpfr_srcptr neg(mpfr_srcptr value) {
        mpfr_ptr out = nextTemp();
        mpfr_neg(out, value, ROUNDING);
        return out;
    }

    FORCE_INLINE mpfr_srcptr abs(mpfr_srcptr value) {
        mpfr_ptr out = nextTemp();
        mpfr_abs(out, value, ROUNDING);
        return out;
    }

    FORCE_INLINE mpfr_srcptr sqrt(mpfr_srcptr value) {
        mpfr_ptr out = nextTemp();
        mpfr_sqrt(out, value, ROUNDING);
        return out;
    }

    FORCE_INLINE mpfr_srcptr pow(mpfr_srcptr a, mpfr_srcptr b) {
        mpfr_ptr out = nextTemp();
        mpfr_pow(out, a, b, ROUNDING);
        return out;
    }

    FORCE_INLINE mpfr_srcptr sin(mpfr_srcptr value) {
        mpfr_ptr out = nextTemp();
        mpfr_sin(out, value, ROUNDING);
        return out;
    }

    FORCE_INLINE mpfr_srcptr cos(mpfr_srcptr value) {
        mpfr_ptr out = nextTemp();
        mpfr_cos(out, value, ROUNDING);
        return out;
    }

    FORCE_INLINE mpfr_srcptr atan2(mpfr_srcptr y, mpfr_srcptr x) {
        mpfr_ptr out = nextTemp();
        mpfr_atan2(out, y, x, ROUNDING);
        return out;
    }

    FORCE_INLINE mpfr_srcptr muladd(mpfr_srcptr a, mpfr_srcptr b, mpfr_srcptr c) {
        mpfr_ptr out = nextTemp();
        mpfr_mul(out, a, b, ROUNDING);
        mpfr_add(out, out, c, ROUNDING);
        return out;
    }

    FORCE_INLINE mpfr_srcptr mulsub(mpfr_srcptr a, mpfr_srcptr b, mpfr_srcptr c) {
        mpfr_ptr out = nextTemp();
        mpfr_mul(out, a, b, ROUNDING);
        mpfr_sub(out, out, c, ROUNDING);
        return out;
    }

    FORCE_INLINE mpfr_srcptr addxx(mpfr_srcptr a, mpfr_srcptr b,
        mpfr_srcptr c, mpfr_srcptr d) {
        mpfr_ptr out = nextTemp();
        mpfr_mul(out, a, b, ROUNDING);

        mpfr_ptr right = nextTemp();
        mpfr_mul(right, c, d, ROUNDING);

        mpfr_add(out, out, right, ROUNDING);
        return out;
    }

    FORCE_INLINE mpfr_srcptr subxx(mpfr_srcptr a, mpfr_srcptr b,
        mpfr_srcptr c, mpfr_srcptr d) {
        mpfr_ptr out = nextTemp();
        mpfr_mul(out, a, b, ROUNDING);

        mpfr_ptr right = nextTemp();
        mpfr_mul(right, c, d, ROUNDING);

        mpfr_sub(out, out, right, ROUNDING);
        return out;
    }

    FORCE_INLINE mpfr_srcptr sgn(mpfr_srcptr value) {
        mpfr_ptr out = nextTemp();
        mpfr_set_si(out, mpfr_sgn(value), ROUNDING);
        return out;
    }

    struct mpfr_num_2_t {
        mpfr_srcptr x;
        mpfr_srcptr y;
    };

    FORCE_INLINE mpfr_num_2_t sincos(mpfr_srcptr value) {
        mpfr_num_2_t out{};

        mpfr_ptr sinOut = nextTemp(), cosOut = nextTemp();
        mpfr_sin_cos(sinOut, cosOut, value, ROUNDING);

        out.x = sinOut;
        out.y = cosOut;
        return out;
    }
}

typedef mpfr_srcptr mpfr_number_t;
typedef mpfr_srcptr mpfr_param_t;
typedef MPFRTypes::mpfr_num_2_t mpfr_num_2_t;

#endif