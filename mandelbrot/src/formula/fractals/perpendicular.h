#pragma once

#include "../FormulaTypes.h"

#define formula_normalPower \
    new_zi = NUM_MULADD(NUM_CONST(-2.0), NUM_MUL(NUM_ABS(zr), zi), ci); \
    new_zr = NUM_ADD(NUM_SUB(zr2, zi2), cr);

#define formula_wholePower \
    number_t f_pzr = NUM_ABS(zr); \
    number_t f_pzi = NUM_NEG(zi); \
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
    const number_t f_pzi = NUM_NEG(zi); \
    \
    const number_t m_rp = NUM_POW(NUM_ADD(zr2, zi2), NUM_HALF(N_VAR)); \
    const number_t m_angle = NUM_MUL(N_VAR, NUM_ATAN2(f_pzi, f_pzr)); \
    \
    const number_2_t m_sincos = NUM_SINCOS(m_angle); \
    new_zr = NUM_MULADD(m_rp, m_sincos.y, cr); \
    new_zi = NUM_MULADD(m_rp, m_sincos.x, ci);

#define derivative_normalPower \
    const number_t t1 = NUM_SUBXX(zr, dr, zi, di); \
    const number_t t2 = NUM_ADDXX(NUM_ABS(zr), di, NUM_MUL(NUM_SGN(zr), zi), dr); \
    \
    new_dr = NUM_MULADD(t1, NUM_CONST(2.0), NUM_VAR(ScalarGlobals::light_r)); \
    new_di = NUM_MULADD(t2, NUM_CONST(-2.0), NUM_VAR(ScalarGlobals::light_i));

#define derivative_wholePower \
    const number_t d_pdr = NUM_MUL(NUM_SGN(zr), dr); \
    const number_t d_pdi = NUM_NEG(di); \
    \
    const number_t d_pzr = NUM_ABS(zr); \
    const number_t d_pzi = NUM_NEG(zi); \
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
    const number_t d_pzi = NUM_NEG(zi); \
    \
    const number_t d_pdr = NUM_MUL(NUM_SGN(zr), dr); \
    const number_t d_pdi = NUM_NEG(di); \
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
