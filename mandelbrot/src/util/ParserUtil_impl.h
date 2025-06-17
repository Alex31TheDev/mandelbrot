#pragma once

#include <cstdlib>
#include <cerrno>
#include <cstdint>

#include <limits>
#include <type_traits>

namespace ParserUtil {
    template<typename T>
    T parseNumber(const std::string &input, bool *ok, const T defaultValue) {
        static_assert(std::is_arithmetic_v<T>, "parseNumber only works with arithmetic types");

        if (input.empty()) {
            if (ok) *ok = false;
            return defaultValue;
        }

        bool isValid = false;

        const char *const numStr = input.c_str();
        char *endPtr = nullptr;

        errno = 0;

        if constexpr (std::is_same_v<T, float>) {
            const float result = strtof(numStr, &endPtr);
            isValid = errno != ERANGE && numStr != endPtr;

            if (ok) *ok = isValid;
            return isValid ? result : defaultValue;
        } else if constexpr (std::is_same_v<T, double>) {
            const double result = strtod(numStr, &endPtr);
            isValid = errno != ERANGE && numStr != endPtr;

            if (ok) *ok = isValid;
            return isValid ? result : defaultValue;
        } else if constexpr (std::is_integral_v<T>) {
            if constexpr (std::is_unsigned_v<T>) {
                const uint64_t result = strtoull(numStr, &endPtr, 10);
                isValid = errno != ERANGE &&
                    numStr != endPtr &&
                    result <= static_cast<uint64_t>(std::numeric_limits<T>::max());

                if (ok) *ok = isValid;
                return isValid ? static_cast<T>(result) : defaultValue;
            } else {
                const int64_t result = strtoll(numStr, &endPtr, 10);
                isValid = errno != ERANGE &&
                    numStr != endPtr &&
                    result >= static_cast<int64_t>(std::numeric_limits<T>::min()) &&
                    result <= static_cast<int64_t>(std::numeric_limits<T>::max());

                if (ok) *ok = isValid;
                return isValid ? static_cast<T>(result) : defaultValue;
            }
        }

        if (ok) *ok = isValid;
        return defaultValue;
    }

    template<typename T>
    T parseNumber(int argc, char *argv[], int index, bool *ok, const T defaultValue) {
        const std::string input = (index < argc) ? argv[index] : "";
        return parseNumber<T>(input, ok, defaultValue);
    }
}
