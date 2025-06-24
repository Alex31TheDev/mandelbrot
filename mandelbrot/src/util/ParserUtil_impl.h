#pragma once

#include <cstdlib>
#include <cerrno>
#include <cstdint>
#include <limits>

namespace ParserUtil {
    template<typename T, int base> requires std::is_arithmetic_v<T>
    T parseNumber(const std::string &input, bool *ok, const T defaultValue) {
        if constexpr (std::is_floating_point_v<T>) {
            static_assert(base == 10,
                "Floats can only be parsed with base 10");
        }

        if (input.empty()) {
            if (ok) *ok = false;
            return defaultValue;
        }

        bool isValid = false;
        T result;

        errno = 0;
        const char *const numStr = input.c_str();
        char *endPtr = nullptr;

        auto validateParse = [&]() -> bool {
            return errno != ERANGE && numStr != endPtr;
            };

        if constexpr (std::is_floating_point_v<T>) {
            if constexpr (std::is_same_v<T, float>) {
                result = strtof(numStr, &endPtr);
            } else if constexpr (std::is_same_v<T, double>) {
                result = strtod(numStr, &endPtr);
            }

            isValid = validateParse();
        } else if constexpr (std::is_integral_v<T>) {
            if constexpr (std::is_unsigned_v<T>) {
                const uint64_t parsed = strtoull(numStr, &endPtr, base);

                isValid = validateParse() &&
                    parsed <= static_cast<uint64_t>(std::numeric_limits<T>::max());

                result = static_cast<T>(parsed);
            } else {
                const int64_t parsed = strtoll(numStr, &endPtr, base);

                isValid = validateParse() &&
                    parsed >= static_cast<int64_t>(std::numeric_limits<T>::min()) &&
                    parsed <= static_cast<int64_t>(std::numeric_limits<T>::max());

                result = static_cast<T>(parsed);
            }
        }

        if (ok) *ok = isValid;
        return isValid ? result : defaultValue;
    }

    template<typename T, int base> requires std::is_arithmetic_v<T>
    T parseNumber(int argc, char *argv[], int index, bool *ok,
        const T defaultValue) {
        const std::string input = (index < argc) ?
            std::string(argv[index]) : "";
        return parseNumber<T, base>(input, ok, defaultValue);
    }
}
