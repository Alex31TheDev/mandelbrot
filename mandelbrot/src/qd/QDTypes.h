#pragma once
#ifdef USE_QD

#include <cctype>
#include <algorithm>
#include <cmath>
#include <string>

#include "qdlib/qd_real.h"

#include "util/InlineUtil.h"

typedef qd_real qd_number_t;
typedef const qd_real &qd_param_t;

struct qd_num_2_t {
    qd_number_t x, y;
};

FORCE_INLINE qd_num_2_t sincos_qd(qd_param_t x) {
    qd_num_2_t out;
    sincos(x, out.x, out.y);
    return out;
}

namespace QDTypes {
    FORCE_INLINE std::string toString(const qd_real &value) {
        if (!isfinite(value)) return "0";
        if (value == qd_real(0.0)) return "0";

        qd_real v = value;
        bool neg = false;
        if (v < qd_real(0.0)) {
            neg = true;
            v = -v;
        }

        int exp10 = static_cast<int>(
            std::floor(std::log10(std::abs(to_double(v))))
            );

        qd_real scaled = v / pow(qd_real(10.0), exp10);
        while (scaled >= qd_real(10.0)) {
            scaled /= qd_real(10.0);
            ++exp10;
        }
        while (scaled < qd_real(1.0)) {
            scaled *= qd_real(10.0);
            --exp10;
        }

        constexpr int digitsCount = 62;
        std::string digits;
        digits.reserve(digitsCount);

        for (int i = 0; i < digitsCount; ++i) {
            int d = static_cast<int>(std::floor(to_double(scaled)));
            d = std::clamp(d, 0, 9);

            digits.push_back(static_cast<char>('0' + d));
            scaled = (scaled - qd_real(static_cast<double>(d))) * qd_real(10.0);
        }

        std::string out;
        out.reserve(digitsCount + 16);

        if (neg) out.push_back('-');
        out.push_back(digits[0]);
        out.push_back('.');
        out.append(digits.begin() + 1, digits.end());

        while (!out.empty() && out.back() == '0') {
            out.pop_back();
        }
        if (!out.empty() && out.back() == '.') {
            out.push_back('0');
        }

        out.push_back('e');
        if (exp10 >= 0) out.push_back('+');
        out.append(std::to_string(exp10));
        return out;
    }

    FORCE_INLINE bool parseNumber(qd_number_t &out, const char *text) {
        if (text == nullptr) return false;

        while (std::isspace(static_cast<unsigned char>(*text))) ++text;
        if (*text == '\0') return false;

        std::string trimmed(text);
        while (!trimmed.empty() &&
            std::isspace(static_cast<unsigned char>(trimmed.back()))) {
            trimmed.pop_back();
        }

        if (trimmed.empty()) return false;

        return qd_real::read(trimmed.c_str(), out) == 0 && isfinite(out);
    }

    FORCE_INLINE bool isValidNumber(const char *text) {
        qd_number_t value;
        return parseNumber(value, text);
    }
}

#endif
