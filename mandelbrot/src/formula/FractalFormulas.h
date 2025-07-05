#pragma once

#include "FormulaTypes.h"

#include "../scalar/ScalarGlobals.h"

#include "../util/InlineUtil.h"

FORCE_INLINE void formula(
    number_param_t cr, number_param_t ci,
    number_param_t zr, number_param_t zi,
    number_param_t mag,
    number_t &new_zr, number_t &new_zi
) {
    using namespace ScalarGlobals;

    if (invalidPower) {
        new_zr = cr;
        new_zi = zi;
    } else if (circlePower) {
        new_zr = NUM_ADD(zr, cr);
        new_zi = NUM_ADD(zi, ci);
        return;
    } else if (normalPower) {
        const number_t zrzi = NUM_MUL(zr, zi);
        new_zr = NUM_ADD(NUM_SUBXX(zr, zr, zi, zi), cr);
        new_zi = NUM_ADD(NUM_DOUBLE(zrzi), ci);
    } else if (wholePower) {
        new_zr = zr;
        new_zi = zi;

        for (int k = 1; k < N; k++) {
            const number_t zrzi = NUM_MUL(new_zr, zi);
            new_zr = NUM_SUBXX(zr, new_zr, zi, new_zi);
            new_zi = NUM_MULADD(zr, new_zi, zrzi);
        }

        new_zr = NUM_ADD(new_zr, cr);
        new_zi = NUM_ADD(new_zi, ci);
    } else {
        const number_t n_var = NUM_VAR(N);
        const number_t rp = NUM_POW(mag, NUM_HALF(n_var));

        const number_t angle = NUM_MUL(n_var, NUM_ATAN2(zi, zr));

        const number_2_t sincos = NUM_SINCOS(angle);
        new_zr = NUM_MULADD(rp, sincos.y, cr);
        new_zi = NUM_MULADD(rp, sincos.x, ci);
    }
}

FORCE_INLINE void derivative(
    number_param_t zr, number_param_t zi,
    number_param_t dr, number_param_t di,
    number_param_t mag,
    number_t &new_dr, number_t &new_di
) {
    using namespace ScalarGlobals;
    const number_t n_var = NUM_VAR(N);

    if (invalidPower) {
        new_dr = new_di = NUM_CONST(0.0);
    } else if (circlePower) {
        new_dr = NUM_CONST(1.0);
        new_di = NUM_CONST(0.0);
    } else if (normalPower) {
        const number_t t1 = NUM_SUBXX(zr, dr, zi, di);
        const number_t t2 = NUM_ADDXX(zr, di, zi, dr);

        new_dr = NUM_MULADD(t1, NUM_CONST(2.0), NUM_CONST(1.0));
        new_di = NUM_DOUBLE(t2);
    } else if (wholePower) {
        new_dr = zr;
        new_di = zi;

        for (int k = 2; k < N; k++) {
            const number_t dizi = NUM_MUL(new_di, zi);
            new_di = NUM_ADDXX(new_dr, zi, new_di, zr);
            new_dr = NUM_MULSUB(new_dr, zr, dizi);
        }

        const number_t t1 = NUM_SUBXX(new_dr, dr, new_di, di);
        const number_t t2 = NUM_ADDXX(new_dr, di, new_di, dr);

        new_dr = NUM_MULADD(t1, n_var, NUM_CONST(1.0));
        new_di = NUM_MUL(t2, n_var);
    } else {
        const number_t pw = NUM_SUB(n_var, NUM_CONST(1.0));
        const number_t rp = NUM_POW(mag, NUM_HALF(pw));

        const number_t angle = NUM_MUL(pw, NUM_ATAN2(zi, zr));

        const number_2_t sincos = NUM_SINCOS(angle);
        new_dr = NUM_MUL(n_var, NUM_MUL(rp, sincos.y));
        new_di = NUM_MUL(n_var, NUM_MUL(rp, sincos.x));

        const number_t t1 = NUM_SUBXX(new_dr, dr, new_di, di);
        const number_t t2 = NUM_ADDXX(new_dr, di, new_di, dr);

        new_dr = NUM_ADD(t1, NUM_CONST(1.0));
        new_di = t2;
    }
}

FORCE_INLINE int formulaOps() {
    using namespace ScalarGlobals;
    int count = 0;

    if (invalidPower) {
        count += 2 * OP_ASSIGN;
    } else if (circlePower) {
        count += 2 * OP_ADD;
    } else if (normalPower) {
        count += OP_MUL;
        count += OP_SUBXX + OP_ADD;
        count += OP_DOUBLE + OP_ADD;
    } else if (wholePower) {
        count += (OP_MUL + OP_SUBXX + OP_MULADD) *
            (static_cast<int>(N) - 1);
        count += 2 * OP_ADD;
    } else {
        count += OP_VAR + OP_HALF;
        count += OP_POW;
        count += OP_ATAN2 + OP_MUL;
        count += OP_SINCOS;
        count += 2 * OP_MULADD;
    }

    return count;
}

FORCE_INLINE int derivativeOps() {
    using namespace ScalarGlobals;
    int count = OP_VAR;

    if (invalidPower) {
        count += 2 * OP_CONST;
    } else if (circlePower) {
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

    return count;
}