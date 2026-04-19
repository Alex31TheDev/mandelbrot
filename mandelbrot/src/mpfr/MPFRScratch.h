#pragma once
#ifdef USE_MPFR

#include <array>

#include <mpfr.h>

#include "MPFRTypes.h"

constexpr int MPFR_SCRATCH_TEMPS = 20;

struct MPFRScratch {
    bool initialized = false;
    mpfr_prec_t precision = 0;

    mpfr_t cr, ci;
    mpfr_t zr, zi;
    mpfr_t dr, di;
    mpfr_t mag;
    mpfr_t zr2, zi2;

    mpfr_t nVal;
    mpfr_t nMinus1;
    mpfr_t halfN;
    mpfr_t halfNMinus1;
    mpfr_t lightR;
    mpfr_t lightI;

    std::array<mpfr_t, MPFR_SCRATCH_TEMPS> t;

    void initAll(mpfr_prec_t bits) {
        precision = bits;

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

        initialized = true;
    }

    void roundAll(mpfr_prec_t bits) {
        precision = bits;
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

    void ensureReady() {
        const mpfr_prec_t bits = MPFRTypes::getPrecisionBits();
        if (!initialized) {
            initAll(bits);
        } else if (precision != bits) {
            roundAll(bits);
        }
    }

    ~MPFRScratch() {
        if (!initialized) return;

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
};

#endif
