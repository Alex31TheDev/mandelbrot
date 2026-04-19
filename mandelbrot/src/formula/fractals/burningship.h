#pragma once

#include "../FormulaTypes.h"

#define formula_normalPower \
    new_zr = NUM_ADD(NUM_SUB(zr2, zi2), cr); \
    new_zi = NUM_MULADD(NUM_CONST(-2.0), NUM_MUL(NUM_ABS(zr), NUM_ABS(zi)), ci);

#define formula_wholePower \
    number_t f_pzr = NUM_ABS(zr); \
    number_t f_pzi = NUM_NEG(NUM_ABS(zi)); \
    \
    number_t f_pzr2 = NUM_MUL(f_pzr, f_pzr); \
    number_t f_pzi2 = NUM_MUL(f_pzi, f_pzi); \
    \
    new_zr = f_pzr; \
    new_zi = f_pzi; \
    \
    for (int k = N - 1; k > 0; k >>= 1) { \
        if ((k & 1) == 1) { \
            const number_t zrzi = NUM_MUL(new_zr, f_pzi); \
            new_zr = NUM_SUBXX(f_pzr, new_zr, f_pzi, new_zi); \
            new_zi = NUM_MULADD(f_pzr, new_zi, zrzi); \
        } \
    \
        f_pzi = NUM_DOUBLE(NUM_MUL(f_pzr, f_pzi)); \
        f_pzr = NUM_SUB(f_pzr2, f_pzi2); \
    \
        f_pzr2 = NUM_MUL(f_pzr, f_pzr); \
        f_pzi2 = NUM_MUL(f_pzi, f_pzi); \
    } \
    \
    new_zr = NUM_ADD(new_zr, cr); \
    new_zi = NUM_ADD(new_zi, ci);

#define formula_fractionalPower \
    const number_t f_pzr = NUM_ABS(zr); \
    const number_t f_pzi = NUM_NEG(NUM_ABS(zi)); \
    \
    const number_t m_rp = NUM_POW(NUM_ADD(zr2, zi2), NUM_HALF(N_VAR)); \
    const number_t m_angle = NUM_MUL(N_VAR, NUM_ATAN2(f_pzi, f_pzr)); \
    \
    const number_2_t m_sincos = NUM_SINCOS(m_angle); \
    new_zr = NUM_MULADD(m_rp, m_sincos.y, cr); \
    new_zi = NUM_MULADD(m_rp, m_sincos.x, ci);

#define derivative_normalPower \
    const number_t t1 = NUM_SUBXX(zr, dr, zi, di); \
    const number_t t2 = NUM_ADDXX(NUM_MUL(NUM_SGN(zr), NUM_ABS(zi)), dr, NUM_MUL(NUM_ABS(zr), NUM_SGN(zi)), di); \
    \
    new_dr = NUM_MULADD(t1, NUM_CONST(2.0), NUM_VAR(ScalarGlobals::light_r)); \
    new_di = NUM_MULADD(t2, NUM_CONST(-2.0), NUM_VAR(ScalarGlobals::light_i));

#define derivative_wholePower \
    const number_t d_pdr = NUM_MUL(NUM_SGN(zr), dr); \
    const number_t d_pdi = NUM_NEG(NUM_MUL(NUM_SGN(zi), di)); \
    \
    const number_t d_pzr = NUM_ABS(zr); \
    const number_t d_pzi = NUM_NEG(NUM_ABS(zi)); \
    \
    new_dr = d_pzr; \
    new_di = d_pzi; \
    \
    for (int k = 2; k < N; k++) { \
        const number_t dizi = NUM_MUL(new_di, d_pzi); \
        new_di = NUM_ADDXX(new_dr, d_pzi, new_di, d_pzr); \
        new_dr = NUM_MULSUB(new_dr, d_pzr, dizi); \
    } \
    \
    const number_t t1 = NUM_SUBXX(new_dr, d_pdr, new_di, d_pdi); \
    const number_t t2 = NUM_ADDXX(new_dr, d_pdi, new_di, d_pdr); \
    \
    new_dr = NUM_MULADD(t1, N_VAR, NUM_VAR(ScalarGlobals::light_r)); \
    new_di = NUM_MULADD(t2, N_VAR, NUM_VAR(ScalarGlobals::light_i));

#define derivative_fractionalPower \
    const number_t d_pw = NUM_SUB(N_VAR, NUM_CONST(1.0)); \
    const number_t d_rp = NUM_POW(mag, NUM_HALF(d_pw)); \
    \
    const number_t d_pzr = NUM_ABS(zr); \
    const number_t d_pzi = NUM_NEG(NUM_ABS(zi)); \
    \
    const number_t d_pdr = NUM_MUL(NUM_SGN(zr), dr); \
    const number_t d_pdi = NUM_NEG(NUM_MUL(NUM_SGN(zi), di)); \
    \
    const number_t d_angle = NUM_MUL(d_pw, NUM_ATAN2(d_pzi, d_pzr)); \
    \
    const number_2_t d_sincos = NUM_SINCOS(d_angle); \
    new_dr = NUM_MUL(N_VAR, NUM_MUL(d_rp, d_sincos.y)); \
    new_di = NUM_MUL(N_VAR, NUM_MUL(d_rp, d_sincos.x)); \
    \
    const number_t t1 = NUM_SUBXX(new_dr, d_pdr, new_di, d_pdi); \
    const number_t t2 = NUM_ADDXX(new_dr, d_pdi, new_di, d_pdr); \
    \
    new_dr = NUM_ADD(t1, NUM_VAR(ScalarGlobals::light_r)); \
    new_di = NUM_ADD(t2, NUM_VAR(ScalarGlobals::light_i));

_FORMULA_OPS_FUNC({});

_DERIVATIVE_OPS_FUNC({});

#ifdef USE_MPFR

static FORCE_INLINE void normalFormula_burningship_mp(mpfr_srcptr cr, mpfr_srcptr ci,
    MPFRScratch &s) {
    mpfr_sub(s.t[0], s.zr2, s.zi2, MPFRTypes::ROUNDING);
    mpfr_add(s.t[0], s.t[0], cr, MPFRTypes::ROUNDING);

    mpfr_abs(s.t[1], s.zr, MPFRTypes::ROUNDING);
    mpfr_abs(s.t[2], s.zi, MPFRTypes::ROUNDING);
    mpfr_mul(s.t[1], s.t[1], s.t[2], MPFRTypes::ROUNDING);
    mpfr_mul_si(s.t[1], s.t[1], -2, MPFRTypes::ROUNDING);
    mpfr_add(s.t[1], s.t[1], ci, MPFRTypes::ROUNDING);

    mpfr_set(s.zr, s.t[0], MPFRTypes::ROUNDING);
    mpfr_set(s.zi, s.t[1], MPFRTypes::ROUNDING);
}

static FORCE_INLINE void wholeFormula_burningship_mp(mpfr_srcptr cr, mpfr_srcptr ci,
    MPFRScratch &s) {
    mpfr_abs(s.t[0], s.zr, MPFRTypes::ROUNDING);
    mpfr_abs(s.t[1], s.zi, MPFRTypes::ROUNDING);
    mpfr_neg(s.t[1], s.t[1], MPFRTypes::ROUNDING);
    wholeFormulaCore_mp(cr, ci, s.t[0], s.t[1], s);
}

static FORCE_INLINE void fractionalFormula_burningship_mp(mpfr_srcptr cr, mpfr_srcptr ci,
    MPFRScratch &s) {
    mpfr_abs(s.t[0], s.zr, MPFRTypes::ROUNDING);
    mpfr_abs(s.t[1], s.zi, MPFRTypes::ROUNDING);
    mpfr_neg(s.t[1], s.t[1], MPFRTypes::ROUNDING);
    fractionalFormulaCore_mp(cr, ci, s.t[0], s.t[1], s);
}

static FORCE_INLINE void normalDerivative_burningship_mp(MPFRScratch &s) {
    mpfr_mul(s.t[0], s.zr, s.dr, MPFRTypes::ROUNDING);
    mpfr_mul(s.t[1], s.zi, s.di, MPFRTypes::ROUNDING);
    mpfr_sub(s.t[2], s.t[0], s.t[1], MPFRTypes::ROUNDING);

    setSign_mp(s.t[3], s.zr);
    mpfr_abs(s.t[4], s.zi, MPFRTypes::ROUNDING);
    mpfr_mul(s.t[3], s.t[3], s.t[4], MPFRTypes::ROUNDING);
    mpfr_mul(s.t[3], s.t[3], s.dr, MPFRTypes::ROUNDING);

    mpfr_abs(s.t[5], s.zr, MPFRTypes::ROUNDING);
    setSign_mp(s.t[6], s.zi);
    mpfr_mul(s.t[5], s.t[5], s.t[6], MPFRTypes::ROUNDING);
    mpfr_mul(s.t[5], s.t[5], s.di, MPFRTypes::ROUNDING);
    mpfr_add(s.t[6], s.t[3], s.t[5], MPFRTypes::ROUNDING);

    mpfr_mul_2ui(s.t[7], s.t[2], 1, MPFRTypes::ROUNDING);
    mpfr_add(s.dr, s.t[7], s.lightR, MPFRTypes::ROUNDING);

    mpfr_mul_2ui(s.t[8], s.t[6], 1, MPFRTypes::ROUNDING);
    mpfr_neg(s.t[8], s.t[8], MPFRTypes::ROUNDING);
    mpfr_add(s.di, s.t[8], s.lightI, MPFRTypes::ROUNDING);
}

static FORCE_INLINE void wholeDerivative_burningship_mp(MPFRScratch &s) {
    const int wholeN = static_cast<int>(ScalarGlobals::N);

    setSign_mp(s.t[0], s.zr);
    mpfr_mul(s.t[0], s.t[0], s.dr, MPFRTypes::ROUNDING);
    setSign_mp(s.t[2], s.zi);
    mpfr_mul(s.t[1], s.t[2], s.di, MPFRTypes::ROUNDING);
    mpfr_neg(s.t[1], s.t[1], MPFRTypes::ROUNDING);

    mpfr_abs(s.t[2], s.zr, MPFRTypes::ROUNDING);
    mpfr_abs(s.t[3], s.zi, MPFRTypes::ROUNDING);
    mpfr_neg(s.t[3], s.t[3], MPFRTypes::ROUNDING);

    mpfr_set(s.t[4], s.t[2], MPFRTypes::ROUNDING);
    mpfr_set(s.t[5], s.t[3], MPFRTypes::ROUNDING);

    for (int k = 2; k < wholeN; ++k) {
        mpfr_mul(s.t[6], s.t[5], s.t[3], MPFRTypes::ROUNDING);

        mpfr_mul(s.t[7], s.t[4], s.t[3], MPFRTypes::ROUNDING);
        mpfr_mul(s.t[8], s.t[5], s.t[2], MPFRTypes::ROUNDING);
        mpfr_add(s.t[5], s.t[7], s.t[8], MPFRTypes::ROUNDING);

        mpfr_mul(s.t[7], s.t[4], s.t[2], MPFRTypes::ROUNDING);
        mpfr_sub(s.t[4], s.t[7], s.t[6], MPFRTypes::ROUNDING);
    }

    mpfr_mul(s.t[6], s.t[4], s.t[0], MPFRTypes::ROUNDING);
    mpfr_mul(s.t[7], s.t[5], s.t[1], MPFRTypes::ROUNDING);
    mpfr_sub(s.t[8], s.t[6], s.t[7], MPFRTypes::ROUNDING);

    mpfr_mul(s.t[6], s.t[4], s.t[1], MPFRTypes::ROUNDING);
    mpfr_mul(s.t[7], s.t[5], s.t[0], MPFRTypes::ROUNDING);
    mpfr_add(s.t[9], s.t[6], s.t[7], MPFRTypes::ROUNDING);

    mpfr_mul(s.t[10], s.t[8], s.nVal, MPFRTypes::ROUNDING);
    mpfr_add(s.dr, s.t[10], s.lightR, MPFRTypes::ROUNDING);
    mpfr_mul(s.t[10], s.t[9], s.nVal, MPFRTypes::ROUNDING);
    mpfr_add(s.di, s.t[10], s.lightI, MPFRTypes::ROUNDING);
}

static FORCE_INLINE void fractionalDerivative_burningship_mp(MPFRScratch &s) {
    mpfr_pow(s.t[0], s.mag, s.halfNMinus1, MPFRTypes::ROUNDING);
    mpfr_abs(s.t[2], s.zr, MPFRTypes::ROUNDING);
    mpfr_abs(s.t[3], s.zi, MPFRTypes::ROUNDING);
    mpfr_neg(s.t[3], s.t[3], MPFRTypes::ROUNDING);
    mpfr_atan2(s.t[1], s.t[3], s.t[2], MPFRTypes::ROUNDING);
    mpfr_mul(s.t[1], s.t[1], s.nMinus1, MPFRTypes::ROUNDING);
    mpfr_sin_cos(s.t[4], s.t[5], s.t[1], MPFRTypes::ROUNDING);

    mpfr_mul(s.t[6], s.t[0], s.t[5], MPFRTypes::ROUNDING);
    mpfr_mul(s.t[7], s.t[0], s.t[4], MPFRTypes::ROUNDING);
    mpfr_mul(s.t[6], s.t[6], s.nVal, MPFRTypes::ROUNDING);
    mpfr_mul(s.t[7], s.t[7], s.nVal, MPFRTypes::ROUNDING);

    setSign_mp(s.t[12], s.zr);
    mpfr_mul(s.t[12], s.t[12], s.dr, MPFRTypes::ROUNDING);
    setSign_mp(s.t[13], s.zi);
    mpfr_mul(s.t[13], s.t[13], s.di, MPFRTypes::ROUNDING);
    mpfr_neg(s.t[13], s.t[13], MPFRTypes::ROUNDING);

    mpfr_mul(s.t[8], s.t[6], s.t[12], MPFRTypes::ROUNDING);
    mpfr_mul(s.t[9], s.t[7], s.t[13], MPFRTypes::ROUNDING);
    mpfr_sub(s.t[10], s.t[8], s.t[9], MPFRTypes::ROUNDING);

    mpfr_mul(s.t[8], s.t[6], s.t[13], MPFRTypes::ROUNDING);
    mpfr_mul(s.t[9], s.t[7], s.t[12], MPFRTypes::ROUNDING);
    mpfr_add(s.t[11], s.t[8], s.t[9], MPFRTypes::ROUNDING);

    mpfr_add(s.dr, s.t[10], s.lightR, MPFRTypes::ROUNDING);
    mpfr_add(s.di, s.t[11], s.lightI, MPFRTypes::ROUNDING);
}

#endif
