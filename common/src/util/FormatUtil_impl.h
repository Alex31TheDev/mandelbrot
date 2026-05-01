#pragma once

#include <sstream>
#include <iomanip>

namespace FormatUtil::_impl {
    inline const char *bigNumberUnits[] = { "", "K", "M", "G", "T" };
    constexpr int bigNumberUnitCount =
        sizeof(bigNumberUnits) / sizeof(bigNumberUnits[0]);
}

namespace FormatUtil {
    template<typename T>
        requires std::is_arithmetic_v<T>
    std::string formatNumber(T value) {
        bool negative = false;
        std::string digits;

        if constexpr (std::is_integral_v<T>) {
            if constexpr (std::is_signed_v<T>) {
                using U = std::make_unsigned_t<T>;
                U mag;

                if (value < 0) mag = U(0) - static_cast<U>(value);
                else mag = static_cast<U>(value);

                digits = std::to_string(mag);
            } else digits = std::to_string(value);
        } else if constexpr (std::is_floating_point_v<T>) {
            negative = std::signbit(value);
            digits = std::to_string(std::abs(value));
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

    template<typename T>
        requires std::is_arithmetic_v<T>
    std::string formatBigNumber(T value) {
        std::ostringstream oss;
        oss << std::fixed;

        double count = 0.0;
        if constexpr (std::is_floating_point_v<T>) {
            if (!std::isfinite(value)) {
                oss << "0" << _impl::bigNumberUnits[0];
                return oss.str();
            }

            count = static_cast<double>(value);
        } else {
            count = static_cast<double>(value);
        }

        const bool negative = count < 0.0;
        count = std::abs(count);

        if (count == 0.0) {
            oss << "0" << _impl::bigNumberUnits[0];
            return oss.str();
        }

        int unitIdx = 0;
        while (count >= 1000.0 && unitIdx < _impl::bigNumberUnitCount - 1) {
            count /= 1000.0;
            unitIdx++;
        }

        count = std::round(count);
        if (count >= 1000.0 && unitIdx < _impl::bigNumberUnitCount - 1) {
            count /= 1000.0;
            unitIdx++;
            count = std::round(count);
        }

        oss << std::setprecision(0);

        if (negative) oss << "-";
        oss << count << _impl::bigNumberUnits[unitIdx];
        return oss.str();
    }
}
