#ifdef USE_QD
#include "CommonDefs.h"
#include "QDGlobals.h"

#include "QDTypes.h"

#include "../scalar/ScalarGlobals.h"
#include "../render/RenderGlobals.h"
using namespace ScalarGlobals;
using namespace RenderGlobals;

namespace QDGlobals {
    qd_number_t aspect_qd;
    qd_number_t halfWidth_qd, halfHeight_qd;
    qd_number_t invWidth_qd, invHeight_qd;
    qd_number_t realScale_qd, imagScale_qd;

    qd_number_t point_r_qd = 0.0, point_i_qd = 0.0;
    qd_number_t seed_r_qd = 0.0, seed_i_qd = 0.0;

    void initQD() {
    }

    void initImageValues() {
        aspect_qd = static_cast<qd_number_t>(width) / height;

        halfWidth_qd = static_cast<qd_number_t>(width) / 2.0;
        halfHeight_qd = static_cast<qd_number_t>(height) / 2.0;

        invWidth_qd = 1.0 / static_cast<qd_number_t>(width);
        invHeight_qd = 1.0 / static_cast<qd_number_t>(height);

        const qd_number_t zoomPow = pow(
            qd_number_t(10.0),
            qd_number_t(zoom)
        );

        realScale_qd = 1.0 / zoomPow;
        imagScale_qd = realScale_qd / aspect_qd;
    }

    void initQDValues(const char *pr_str, const char *pi_str) {
        initImageValues();

        if (!QDTypes::parseNumber(point_r_qd, pr_str)) point_r_qd = 0.0;
        if (!QDTypes::parseNumber(point_i_qd, pi_str)) point_i_qd = 0.0;

        seed_r_qd = seed_r;
        seed_i_qd = seed_i;
    }
}

#endif
