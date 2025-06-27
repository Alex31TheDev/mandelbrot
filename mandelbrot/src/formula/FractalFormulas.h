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
        new_zr = NUM_ADD(NUM_MULSUB(zr, zr, NUM_MUL(zi, zi)), cr);
        new_zi = NUM_ADD(NUM_ADD(zrzi, zrzi), ci);
    } else if (wholePower) {
        new_zr = zr;
        new_zi = zi;

        for (int k = 2; k < N; k++) {
            const number_t zrzi = NUM_MUL(new_zr, zi);
            new_zr = NUM_MULSUB(zr, new_zr, NUM_MUL(zi, new_zi));
            new_zi = NUM_MULADD(zr, new_zi, zrzi);
        }

        new_zr = NUM_ADD(new_zr, cr);
        new_zi = NUM_ADD(new_zi, ci);
    } else {
        const number_t n_var = NUM_VAR(N);

        const number_t rp = NUM_POW(mag,
            NUM_MUL(n_var, NUM_CONST(0.5))
        );

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
        const number_t t1 = NUM_MULSUB(zr, dr, NUM_MUL(zi, di));
        const number_t t2 = NUM_MULADD(zr, di, NUM_MUL(zi, dr));

        new_dr = NUM_ADD(NUM_ADD(t1, t1), NUM_CONST(1.0));
        new_di = NUM_ADD(t2, t2);
    } else if (wholePower) {
        new_dr = zr;
        new_di = zi;

        for (int k = 2; k < N; k++) {
            const number_t dizi = NUM_MUL(new_di, zi);
            new_di = NUM_MULADD(new_dr, zi, NUM_MUL(new_di, zr));
            new_dr = NUM_MULSUB(new_dr, zr, dizi);
        }

        const number_t t1 = NUM_MULSUB(new_dr, dr, NUM_MUL(new_di, di));
        const number_t t2 = NUM_MULADD(new_dr, di, NUM_MUL(new_di, dr));

        new_dr = NUM_MULADD(t1, n_var, NUM_CONST(1.0));
        new_di = NUM_MUL(t2, n_var);
    } else {
        const number_t pw = NUM_SUB(n_var, NUM_CONST(1.0));
        const number_t rp = NUM_POW(mag, NUM_DIV(pw, NUM_CONST(2.0)));

        const number_t angle = NUM_MUL(pw, NUM_ATAN2(zi, zr));

        const number_2_t sincos = NUM_SINCOS(angle);
        new_dr = NUM_MUL(n_var, NUM_MUL(rp, sincos.y));
        new_di = NUM_MUL(n_var, NUM_MUL(rp, sincos.x));

        const number_t t1 = NUM_MULSUB(new_dr, dr, NUM_MUL(new_di, di));
        const number_t t2 = NUM_MULADD(new_dr, di, NUM_MUL(new_di, dr));

        new_dr = NUM_ADD(t1, NUM_CONST(1.0));
        new_di = t2;
    }
}
