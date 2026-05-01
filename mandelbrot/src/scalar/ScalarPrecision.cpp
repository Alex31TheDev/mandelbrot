#include "CommonDefs.h"
#include "ScalarPrecision.h"

#include <algorithm>

#include "ScalarGlobals.h"
#include "render/RenderGlobals.h"
using namespace ScalarGlobals;
using namespace RenderGlobals;

#include "util/RangeUtil.h"

namespace ScalarPrecision {
    DEFINE_RANGE_ARRAY(Threshold, lossThresholds,
        { Precision::half, SC_SYM_F(3.7e7) },
        { Precision::full, SC_SYM_F(2.0e16) },
        { Precision::arbitrary, SC_SYM_F(4.0e19) }
    );

    Precision precisionRank() {
        const scalar_full_t mag = MAX_F(ABS_F(point_r), ABS_F(point_i)),
            minDim = MIN_F(CAST_F(width), CAST_F(height)),
            metric = mag * minDim / realScale;

        if (!std::isfinite(static_cast<double>(metric))) {
            return Precision::arbitrary;
        }

        lossThresholdsRange range{};
        const auto it = std::ranges::find_if(range,
            [metric](const Threshold &threshold) {
                return metric <= threshold.value;
            });

        return (it != range.end()) ? it->precision : Precision::arbitrary;
    }
}
