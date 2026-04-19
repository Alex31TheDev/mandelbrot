#pragma once
#ifdef USE_QD

#include <cctype>
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
