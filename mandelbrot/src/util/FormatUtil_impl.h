#pragma once

#include <cmath>
#include <sstream>

namespace FormatUtil {
    template<typename T>
        requires std::is_arithmetic_v<T>
    std::string formatNumber(T value) {
        bool negative;
        std::string digits;

        if constexpr (std::is_integral_v<T>) {
            negative = value < 0;
            digits = std::to_string(negative ? -value : value);
        } else if constexpr (std::is_floating_point_v<T>) {
            negative = signbit(value);
            digits = std::to_string(abs(value));
        }

        const size_t dotPos = digits.find('.');

        const std::string intPart = digits.substr(0, dotPos);
        const std::string fracPart = (dotPos == std::string::npos)
            ? "" : digits.substr(dotPos);

        const int len = static_cast<int>(intPart.size());
        const int firstGroup = (len + 2) % 3 + 1;

        std::ostringstream oss;
        if (negative) oss << '-';

        oss << intPart.substr(0, firstGroup);

        for (int i = firstGroup; i < len; i += 3) {
            oss << ',' << intPart.substr(i, 3);
        }

        oss << fracPart;
        return oss.str();
    }
}
