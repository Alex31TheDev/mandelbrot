#ifdef USE_QD
#include "CommonDefs.h"
#include "QDGlobals.h"

#include <string>

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
    qd_number_t zoom_qd = 0.0;

    qd_number_t point_r_qd = 0.0, point_i_qd = 0.0;
    qd_number_t seed_r_qd = 0.0, seed_i_qd = 0.0;

    void initQD() {}
}
static std::string qdZoomString = "0";

namespace QDGlobals {
    void initImageValues() {
        if (!QDTypes::parseNumber(zoom_qd, qdZoomString.c_str())) {
            zoom_qd = static_cast<qd_number_t>(zoom);
        }

        aspect_qd = static_cast<qd_number_t>(width) / height;

        halfWidth_qd = static_cast<qd_number_t>(width) / 2.0;
        halfHeight_qd = static_cast<qd_number_t>(height) / 2.0;

        invWidth_qd = 1.0 / static_cast<qd_number_t>(width);
        invHeight_qd = 1.0 / static_cast<qd_number_t>(height);

        const qd_number_t zoomPow = pow(
            qd_number_t(10.0),
            zoom_qd
        );

        realScale_qd = 1.0 / zoomPow;
        imagScale_qd = realScale_qd / aspect_qd;
    }

    void initQDValues(
        const char *pr_str, const char *pi_str,
        const char *zoomStr, const char *sr_str, const char *si_str
    ) {
        if (zoomStr && *zoomStr) qdZoomString = zoomStr;
        initImageValues();

        if (!QDTypes::parseNumber(point_r_qd, pr_str)) point_r_qd = 0.0;
        if (!QDTypes::parseNumber(point_i_qd, pi_str)) point_i_qd = 0.0;

        if (!QDTypes::parseNumber(seed_r_qd, sr_str)) seed_r_qd = 0.0;
        if (!QDTypes::parseNumber(seed_i_qd, si_str)) seed_i_qd = 0.0;
    }
}

#endif