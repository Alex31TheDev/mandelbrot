#pragma once

#include <string_view>

namespace NumberUtil {
    bool almostEqual(float a, float b, float epsilon = 1e-6f);
    bool almostEqual(double a, double b, double epsilon = 1e-12);

    bool equalParsedDoubleText(std::string_view a,
        std::string_view b,
        double epsilon = 1e-12);
}
