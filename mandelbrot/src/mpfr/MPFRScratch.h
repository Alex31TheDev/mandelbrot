#pragma once
#ifdef USE_MPFR

#include <array>

#include <mpfr.h>

#include "MPFRTypes.h"

constexpr int MPFR_SCRATCH_TEMPS = 20;

class MPFRScratch {
public:
    MPFRScratch() = default;
    ~MPFRScratch();

    MPFRScratch(const MPFRScratch &) = delete;
    MPFRScratch &operator=(const MPFRScratch &) = delete;

    void ensureReady();

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

private:
    void initAll(mpfr_prec_t bits);
    void roundAll(mpfr_prec_t bits);

    bool _initialized = false;
    mpfr_prec_t _precision = 0;
};

#endif
