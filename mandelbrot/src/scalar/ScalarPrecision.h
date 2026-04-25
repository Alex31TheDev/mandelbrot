#pragma once
#include "CommonDefs.h"

#include "ScalarTypes.h"
#include "util/RangeUtil.h"

namespace ScalarPrecision {
    enum class Precision {
        half = 0,
        full = 1,
        arbitrary = 2
    };

    struct Threshold {
        Precision precision;
        scalar_full_t value;
    };

    DECLARE_RANGE_ARRAY(Threshold, lossThresholds);

    Precision precisionRank();
}