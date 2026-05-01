#include "NumberUtil.h"

#include <cmath>
#include <functional>
#include <string>
#include <string_view>

#include "StringUtil.h"
#include "ParserUtil.h"

static bool tryParseDouble(std::string_view value, double &out) {
    const std::string_view trimmed = StringUtil::trimWhitespace(value);
    if (trimmed.empty()) return false;

    bool ok = false;
    out = ParserUtil::parseNumber<double>(
        std::string(trimmed), std::ref(ok), 0.0);
    return ok;
}

namespace NumberUtil {
    bool almostEqual(float a, float b, float epsilon) {
        return std::fabs(a - b) <= epsilon;
    }

    bool almostEqual(double a, double b, double epsilon) {
        return std::fabs(a - b) <= epsilon;
    }

    bool equalParsedDoubleText(
        std::string_view a, std::string_view b,
        double epsilon
    ) {
        if (StringUtil::trimWhitespace(a) ==
            StringUtil::trimWhitespace(b)) {
            return true;
        }

        double parsedA = 0.0, parsedB = 0.0;
        if (!tryParseDouble(a, parsedA) || !tryParseDouble(b, parsedB)) {
            return false;
        }

        if (!std::isfinite(parsedA) || !std::isfinite(parsedB)) {
            return parsedA == parsedB;
        }

        return almostEqual(parsedA, parsedB, epsilon);
    }
}
