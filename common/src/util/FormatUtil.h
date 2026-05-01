#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <functional>

namespace FormatUtil {
    template<typename T>
        requires std::is_arithmetic_v<T>
    std::string formatNumber(T x);
    template<typename T>
        requires std::is_arithmetic_v<T>
    std::string formatBigNumber(T x);
    std::string formatBool(bool value);

    std::string formatBufferSize(size_t size);
    std::string formatDuration(int64_t millis);
    std::string formatHexColor(float r, float g, float b);

    std::string formatIndexedName(std::string_view baseName, int index);
    std::string uniqueIndexedName(std::string_view baseName,
        const std::function<bool(std::string_view)> &nameExists,
        int startIndex = 1);
}

#include "FormatUtil_impl.h"
