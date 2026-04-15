#pragma once

#include "../FormulaTypes.h"

#define formula_normalPower \
    new_zr = NUM_ADD(NUM_SUB(zr2, zi2), cr); \
    new_zi = NUM_MULADD(NUM_CONST(2.0), NUM_MUL(zr, zi), ci); 

#define formula_wholePower \
    for (int k = N - 1; k > 0; k >>= 1) { \
        if ((k & 1) == 1) { \
            const number_t zrzi = NUM_MUL(new_zr, zi); \
            new_zr = NUM_SUBXX(zr, new_zr, zi, new_zi); \
            new_zi = NUM_MULADD(zr, new_zi, zrzi); \
        } \
    \
        zi = NUM_DOUBLE(NUM_MUL(zr, zi)); \
        zr = NUM_SUB(zr2, zi2); \
    \
        zr2 = NUM_MUL(zr, zr); \
        zi2 = NUM_MUL(zi, zi); \
    } \
    \
        new_zr = NUM_ADD(new_zr, cr); \
        new_zi = NUM_ADD(new_zi, ci);

#define formula_fractionalPower \
    const number_t m_rp = NUM_POW(NUM_ADD(zr2, zi2), NUM_HALF(N_VAR)); \
    const number_t m_angle = NUM_MUL(N_VAR, NUM_ATAN2(zi, zr)); \
    \
    const number_2_t m_sincos = NUM_SINCOS(m_angle); \
    new_zr = NUM_MULADD(m_rp, m_sincos.y, cr); \
    new_zi = NUM_MULADD(m_rp, m_sincos.x, ci); 

#define derivative_normalPower \
    const number_t t1 = NUM_SUBXX(zr, dr, zi, di); \
    const number_t t2 = NUM_ADDXX(zr, di, zi, dr); \
    \
    new_dr = NUM_MULADD(t1, NUM_CONST(2.0), NUM_CONST(1.0)); \
    new_di = NUM_DOUBLE(t2);

#define derivative_wholePower \
    new_dr = zr; \
    new_di = zi; \
    \
    for (int k = 2; k < N; k++) { \
        const number_t dizi = NUM_MUL(new_di, zi); \
        new_di = NUM_ADDXX(new_dr, zi, new_di, zr); \
        new_dr = NUM_MULSUB(new_dr, zr, dizi); \
    } \
    \
    const number_t t1 = NUM_SUBXX(new_dr, dr, new_di, di); \
    const number_t t2 = NUM_ADDXX(new_dr, di, new_di, dr); \
    \
    new_dr = NUM_MULADD(t1, N_VAR, NUM_CONST(1.0)); \
    new_di = NUM_MUL(t2, N_VAR); 

#define derivative_fractionalPower \
    const number_t d_pw = NUM_SUB(N_VAR, NUM_CONST(1.0)); \
    const number_t d_rp = NUM_POW(mag, NUM_HALF(d_pw)); \
    \
    const number_t d_angle = NUM_MUL(d_pw, NUM_ATAN2(zi, zr)); \
    \
    const number_2_t d_sincos = NUM_SINCOS(d_angle); \
    new_dr = NUM_MUL(N_VAR, NUM_MUL(d_rp, d_sincos.y)); \
    new_di = NUM_MUL(N_VAR, NUM_MUL(d_rp, d_sincos.x)); \
    \
    const number_t t1 = NUM_SUBXX(new_dr, dr, new_di, di); \
    const number_t t2 = NUM_ADDXX(new_dr, di, new_di, dr); \
    \
    new_dr = NUM_ADD(t1, NUM_CONST(1.0)); \
    new_di = t2;

_FORMULA_OPS_FUNC(
    {
        if (invalidPower) {
            count += 2 * OP_ASSIGN;
        } else if (normalPower) {
            count += OP_CONST + OP_MULADD + OP_ADD;
            count += OP_SUB + OP_ADD;
        } else if (wholePower) {
            for (int k = N - 1; k > 0; k >>= 1) {
                if ((k & 1) == 1) {
                    count += OP_MUL;
                    count += OP_SUBXX;
                    count += OP_MULADD;
                }

                count += OP_DOUBLE + OP_MUL;
                count += OP_SUB;

                count += 2 * OP_MUL;
            }

            count += 2 * OP_ADD;
        } else {
            count += OP_VAR + OP_HALF;
            count += OP_POW;
            count += OP_ATAN2 + OP_MUL;
            count += OP_SINCOS;
            count += 2 * OP_MULADD;
        }
    }
);

_DERIVATIVE_OPS_FUNC(
    {
        count += OP_VAR;

        if (invalidPower) {
            count += 2 * OP_CONST;
        } else if (normalPower) {
            count += OP_SUBXX + OP_ADDXX;
            count += OP_CONST + OP_CONST + OP_MULADD;
            count += OP_DOUBLE;
        } else if (wholePower) {
            count += (OP_MUL + OP_ADDXX + OP_MULSUB) *
                (static_cast<int>(N) - 2);
            count += OP_SUBXX + OP_ADDXX;
            count += OP_CONST + OP_MULADD;
            count += OP_MUL;
        } else {
            count += OP_CONST + OP_SUB;
            count += OP_HALF + OP_POW;
            count += OP_ATAN2 + OP_MUL;
            count += OP_SINCOS;
            count += 4 * OP_MUL;
            count += OP_SUBXX + OP_ADDXX;
            count += OP_CONST + OP_ADD;
        }
    }
);