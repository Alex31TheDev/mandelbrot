#pragma once

#include <cstdint>
#include <string>
#include <type_traits>

namespace FormatUtil {
    template<typename T>
        requires std::is_arithmetic_v<T>
    std::string formatNumber(T x);

    std::string formatBufferSize(size_t size);
    std::string formatDuration(int64_t millis, int decimals = 1);
}

#include "FormatUtil_impl.h"