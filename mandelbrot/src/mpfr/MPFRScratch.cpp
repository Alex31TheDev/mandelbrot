#ifdef USE_MPFR
#include "MPFRScratch.h"

void MPFRScratch::initAll(mpfr_prec_t bits) {
    _precision = bits;

    auto initOne = [bits](mpfr_t value) {
        mpfr_init2(value, bits);
        };

    initOne(cr); initOne(ci);
    initOne(zr); initOne(zi);
    initOne(dr); initOne(di);
    initOne(mag);
    initOne(zr2); initOne(zi2);

    initOne(nVal);
    initOne(nMinus1);
    initOne(halfN);
    initOne(halfNMinus1);
    initOne(lightR);
    initOne(lightI);

    for (auto &tmp : t) {
        initOne(tmp);
    }

    _initialized = true;
}

void MPFRScratch::roundAll(mpfr_prec_t bits) {
    _precision = bits;

    auto roundOne = [bits](mpfr_t value) {
        mpfr_prec_round(value, bits, MPFRTypes::ROUNDING);
        };

    roundOne(cr); roundOne(ci);
    roundOne(zr); roundOne(zi);
    roundOne(dr); roundOne(di);
    roundOne(mag);
    roundOne(zr2); roundOne(zi2);

    roundOne(nVal);
    roundOne(nMinus1);
    roundOne(halfN);
    roundOne(halfNMinus1);
    roundOne(lightR);
    roundOne(lightI);

    for (auto &tmp : t) {
        roundOne(tmp);
    }
}

void MPFRScratch::ensureReady() {
    const mpfr_prec_t bits = MPFRTypes::getPrecisionBits();
    if (!_initialized) {
        initAll(bits);
    } else if (_precision != bits) {
        roundAll(bits);
    }
}

MPFRScratch::~MPFRScratch() {
    if (!_initialized) return;

    mpfr_clear(cr); mpfr_clear(ci);
    mpfr_clear(zr); mpfr_clear(zi);
    mpfr_clear(dr); mpfr_clear(di);
    mpfr_clear(mag);
    mpfr_clear(zr2); mpfr_clear(zi2);

    mpfr_clear(nVal);
    mpfr_clear(nMinus1);
    mpfr_clear(halfN);
    mpfr_clear(halfNMinus1);
    mpfr_clear(lightR);
    mpfr_clear(lightI);

    for (auto &tmp : t) {
        mpfr_clear(tmp);
    }
}

#endif