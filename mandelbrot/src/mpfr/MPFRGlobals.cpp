#ifdef USE_MPFR
#include "CommonDefs.h"
#include "MPFRGlobals.h"

#include <algorithm>
#include <cmath>
#include <string>

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
    mpfr_t zoom_mp;

    mpfr_t point_r_mp, point_i_mp;
    mpfr_t seed_r_mp, seed_i_mp;
}

static bool mpfrGlobalsInitialized = false;
static std::string mpfrZoomString = "0";

static void initOne(mpfr_t value) {
    if (!mpfrGlobalsInitialized) {
        mpfr_init2(value, MPFRTypes::precisionBits);
        return;
    }

    if (mpfr_get_prec(value) != MPFRTypes::precisionBits) {
        mpfr_prec_round(value, MPFRTypes::precisionBits, MPFRTypes::ROUNDING);
    }
}

static void ensureStorage() {
    initOne(MPFRGlobals::bailout_mp);
    initOne(MPFRGlobals::aspect_mp);
    initOne(MPFRGlobals::halfWidth_mp);
    initOne(MPFRGlobals::halfHeight_mp);
    initOne(MPFRGlobals::invWidth_mp);
    initOne(MPFRGlobals::invHeight_mp);
    initOne(MPFRGlobals::realScale_mp);
    initOne(MPFRGlobals::imagScale_mp);
    initOne(MPFRGlobals::zoom_mp);
    initOne(MPFRGlobals::point_r_mp);
    initOne(MPFRGlobals::point_i_mp);
    initOne(MPFRGlobals::seed_r_mp);
    initOne(MPFRGlobals::seed_i_mp);

    mpfrGlobalsInitialized = true;
}

namespace MPFRGlobals {
    int precisionDigits() {
        ensureStorage();

        const int safeHeight = std::max(1, height);
        mpfr_t pixelSpacing_mp;
        mpfr_t ten_mp;
        mpfr_t negZoom_mp;
        mpfr_t height_mp;
        mpfr_inits2(MPFRTypes::precisionBits,
            pixelSpacing_mp, ten_mp, negZoom_mp, height_mp,
            static_cast<mpfr_ptr>(nullptr));

        mpfr_set_d(ten_mp, 10.0, MPFRTypes::ROUNDING);
        if (!MPFRTypes::parseNumber(zoom_mp, mpfrZoomString.c_str())) {
            mpfr_set_d(zoom_mp, static_cast<double>(zoom), MPFRTypes::ROUNDING);
        }
        mpfr_neg(negZoom_mp, zoom_mp, MPFRTypes::ROUNDING);
        mpfr_pow(pixelSpacing_mp, ten_mp, negZoom_mp, MPFRTypes::ROUNDING);
        mpfr_set_si(height_mp, safeHeight, MPFRTypes::ROUNDING);
        mpfr_div(pixelSpacing_mp, pixelSpacing_mp, height_mp, MPFRTypes::ROUNDING);

        if (!(mpfr_cmp_ui(pixelSpacing_mp, 0) > 0) ||
            !mpfr_number_p(pixelSpacing_mp)) {
            const double digitsFromBits = std::ceil(
                static_cast<double>(MPFRTypes::precisionBits) * LOG10_2);
            mpfr_clears(pixelSpacing_mp, ten_mp, negZoom_mp, height_mp,
                static_cast<mpfr_ptr>(nullptr));
            return std::max(MIN_PRECISION_DIGITS,
                static_cast<int>(digitsFromBits));
        }

        const int exponent = mpfr_get_exp(pixelSpacing_mp);
        mpfr_clears(pixelSpacing_mp, ten_mp, negZoom_mp, height_mp,
            static_cast<mpfr_ptr>(nullptr));

        const double digitsEstimate =
            static_cast<double>(MIN_PRECISION_DIGITS) +
            LOG10_2 * static_cast<double>(-exponent);

        return std::max(MIN_PRECISION_DIGITS,
            static_cast<int>(std::ceil(digitsEstimate)));
    }

    void initMPFR(int prec) {
        if (prec <= 0) prec = precisionDigits();

        MPFRTypes::setPrecisionBits(MPFRTypes::digits2bits(prec));
        ensureStorage();

        mpfr_set_d(bailout_mp, static_cast<double>(BAILOUT), MPFRTypes::ROUNDING);
    }

    void initImageValues() {
        ensureStorage();

        mpfr_t width_mp, height_mp, ten_mp, zoomPow_mp;
        mpfr_init2(width_mp, MPFRTypes::precisionBits);
        mpfr_init2(height_mp, MPFRTypes::precisionBits);
        mpfr_init2(ten_mp, MPFRTypes::precisionBits);
        mpfr_init2(zoomPow_mp, MPFRTypes::precisionBits);

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
        if (!MPFRTypes::parseNumber(zoom_mp, mpfrZoomString.c_str())) {
            mpfr_set_d(zoom_mp, static_cast<double>(zoom), MPFRTypes::ROUNDING);
        }
        mpfr_pow(zoomPow_mp, ten_mp, zoom_mp, MPFRTypes::ROUNDING);

        mpfr_ui_div(realScale_mp, 1, zoomPow_mp, MPFRTypes::ROUNDING);
        mpfr_div(imagScale_mp, realScale_mp, aspect_mp, MPFRTypes::ROUNDING);

        mpfr_clear(width_mp);
        mpfr_clear(height_mp);
        mpfr_clear(ten_mp);
        mpfr_clear(zoomPow_mp);
    }

    void initMPFRValues(const char *pr_str, const char *pi_str,
        const char *zoomStr, const char *sr_str, const char *si_str) {
        ensureStorage();
        if (zoomStr && *zoomStr) mpfrZoomString = zoomStr;
        initImageValues();

        if (!MPFRTypes::parseNumber(point_r_mp, pr_str)) {
            mpfr_set_zero(point_r_mp, 0);
        }
        if (!MPFRTypes::parseNumber(point_i_mp, pi_str)) {
            mpfr_set_zero(point_i_mp, 0);
        }

        if (!MPFRTypes::parseNumber(seed_r_mp, sr_str)) {
            mpfr_set_zero(seed_r_mp, 0);
        }
        if (!MPFRTypes::parseNumber(seed_i_mp, si_str)) {
            mpfr_set_zero(seed_i_mp, 0);
        }
    }
}

#endif
