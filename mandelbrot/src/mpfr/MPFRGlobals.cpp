#ifdef USE_MPFR
#include "CommonDefs.h"
#include "MPFRGlobals.h"

#include "MPFRTypes.h"

#include "../scalar/ScalarGlobals.h"
#include "../render/RenderGlobals.h"
using namespace ScalarGlobals;
using namespace RenderGlobals;

namespace MPFRGlobals {
    mpfr_t bailout_mp;
    mpfr_t aspect_mp;
    mpfr_t halfWidth_mp, halfHeight_mp;
    mpfr_t invWidth_mp, invHeight_mp;
    mpfr_t realScale_mp, imagScale_mp;

    mpfr_t point_r_mp, point_i_mp;
    mpfr_t seed_r_mp, seed_i_mp;

    static bool _initialized = false;

    static void initOne(mpfr_t value) {
        if (!_initialized) {
            mpfr_init2(value, MPFRTypes::getPrecisionBits());
            return;
        }

        if (mpfr_get_prec(value) != MPFRTypes::getPrecisionBits()) {
            mpfr_prec_round(value, MPFRTypes::getPrecisionBits(), MPFRTypes::ROUNDING);
        }
    }

    static void ensureStorage() {
        initOne(bailout_mp);
        initOne(aspect_mp);
        initOne(halfWidth_mp);
        initOne(halfHeight_mp);
        initOne(invWidth_mp);
        initOne(invHeight_mp);
        initOne(realScale_mp);
        initOne(imagScale_mp);
        initOne(point_r_mp);
        initOne(point_i_mp);
        initOne(seed_r_mp);
        initOne(seed_i_mp);

        _initialized = true;
    }

    void initMPFR(int prec) {
        if (prec <= 0) prec = digits;

        MPFRTypes::setPrecisionBits(MPFRTypes::digits2bits(prec));
        ensureStorage();

        mpfr_set_d(bailout_mp, static_cast<double>(BAILOUT), MPFRTypes::ROUNDING);
    }

    void initImageValues() {
        mpfr_t width_mp, height_mp, zoom_mp, ten_mp, zoomPow_mp;
        mpfr_init2(width_mp, MPFRTypes::getPrecisionBits());
        mpfr_init2(height_mp, MPFRTypes::getPrecisionBits());
        mpfr_init2(zoom_mp, MPFRTypes::getPrecisionBits());
        mpfr_init2(ten_mp, MPFRTypes::getPrecisionBits());
        mpfr_init2(zoomPow_mp, MPFRTypes::getPrecisionBits());

        mpfr_set_si(width_mp, width, MPFRTypes::ROUNDING);
        mpfr_set_si(height_mp, height, MPFRTypes::ROUNDING);

        mpfr_div(aspect_mp, width_mp, height_mp, MPFRTypes::ROUNDING);

        mpfr_set(halfWidth_mp, width_mp, MPFRTypes::ROUNDING);
        mpfr_div_2ui(halfWidth_mp, halfWidth_mp, 1, MPFRTypes::ROUNDING);

        mpfr_set(halfHeight_mp, height_mp, MPFRTypes::ROUNDING);
        mpfr_div_2ui(halfHeight_mp, halfHeight_mp, 1, MPFRTypes::ROUNDING);

        mpfr_ui_div(invWidth_mp, 1, width_mp, MPFRTypes::ROUNDING);
        mpfr_ui_div(invHeight_mp, 1, height_mp, MPFRTypes::ROUNDING);

        mpfr_set_d(ten_mp, 10.0, MPFRTypes::ROUNDING);
        mpfr_set_d(zoom_mp, static_cast<double>(zoom), MPFRTypes::ROUNDING);
        mpfr_pow(zoomPow_mp, ten_mp, zoom_mp, MPFRTypes::ROUNDING);

        mpfr_ui_div(realScale_mp, 1, zoomPow_mp, MPFRTypes::ROUNDING);
        mpfr_div(imagScale_mp, realScale_mp, aspect_mp, MPFRTypes::ROUNDING);

        mpfr_clear(width_mp);
        mpfr_clear(height_mp);
        mpfr_clear(zoom_mp);
        mpfr_clear(ten_mp);
        mpfr_clear(zoomPow_mp);
    }

    void initMPFRValues(const char *pr_str, const char *pi_str) {
        ensureStorage();
        initImageValues();

        if (!MPFRTypes::parseNumber(point_r_mp, pr_str)) {
            mpfr_set_zero(point_r_mp, 0);
        }
        if (!MPFRTypes::parseNumber(point_i_mp, pi_str)) {
            mpfr_set_zero(point_i_mp, 0);
        }

        mpfr_set_d(seed_r_mp, static_cast<double>(seed_r), MPFRTypes::ROUNDING);
        mpfr_set_d(seed_i_mp, static_cast<double>(seed_i), MPFRTypes::ROUNDING);
    }
}

#endif