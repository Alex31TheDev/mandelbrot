#pragma once

#if defined(USE_SCALAR)
#include <cmath>
#elif defined(USE_VECTORS)
#include <immintrin.h>
#endif

#include "FormulaTypes.h"

#include "../scalar/ScalarGlobals.h"

#include "../util/InlineUtil.h"

static FORCE_INLINE void formula(const number_t &cr, const number_t &ci,
    const number_t &zr, const number_t &zi,
    const number_t &zr2, const number_t &zi2, const number_t &mag,
    number_t &new_zr, number_t &new_zi) {
    using namespace ScalarGlobals;

    if (N <= 0) {
        new_zr = cr;
        new_zi = zi;
    } else if (N == 1) {
        new_zr = NUM_ADD(zr, cr);
        new_zi = NUM_ADD(zi, ci);
        return;
    } else if (N == 2) {
        const number_t zrzi = NUM_MUL(zr, zi);
        new_zr = NUM_ADD(NUM_SUB(zr2, zi2), cr);
        new_zi = NUM_ADD(NUM_ADD(zrzi, zrzi), ci);
    } else if (static_cast<int>(N) == N) {
        number_t zrzi = NUM_MUL(zr, zi);
        new_zr = NUM_SUB(zr2, zi2);
        new_zi = NUM_ADD(zrzi, zrzi);

        for (int k = 2; k < N; k++) {
            zrzi = NUM_MUL(new_zr, zi);
            new_zr = NUM_SUB(NUM_MUL(zr, new_zr), NUM_MUL(zi, new_zi));
            new_zi = NUM_ADD(zrzi, NUM_MUL(zr, new_zi));
        }

        new_zr = NUM_ADD(new_zr, cr);
        new_zi = NUM_ADD(new_zi, ci);
    } else {
        const number_t rp = NUM_POW(mag, NUM_DIV(NUM_VAR(N), NUM_CONST(2.0)));

        const number_t theta = NUM_ATAN2(zi, zr);
        const number_t angle = NUM_MUL(NUM_VAR(N), theta);

        new_zr = NUM_ADD(NUM_MUL(rp, NUM_COS(angle)), cr);
        new_zi = NUM_ADD(NUM_MUL(rp, NUM_SIN(angle)), ci);
    }
}

static FORCE_INLINE void derivative(const number_t &zr, const number_t &zi,
    const number_t &dr, const number_t &di,
    const number_t &mag,
    number_t &new_dr, number_t &new_di) {
    using namespace ScalarGlobals;

    if (N <= 0) {
        new_dr = new_di = NUM_CONST(0.0);
    } else if (N == 1) {
        new_dr = NUM_CONST(1.0);
        new_di = NUM_CONST(0.0);
    } else if (N == 2) {
        const number_t t1 = NUM_SUB(NUM_MUL(zr, dr), NUM_MUL(zi, di));
        const number_t t2 = NUM_ADD(NUM_MUL(zr, di), NUM_MUL(zi, dr));

        new_dr = NUM_ADD(NUM_ADD(t1, t1), NUM_CONST(1.0));
        new_di = NUM_ADD(t2, t2);
    } else if (static_cast<int>(N) == N) {
        new_dr = zr;
        new_di = zi;

        for (int k = 2; k < N; k++) {
            const number_t dizi = NUM_MUL(new_di, zi);
            new_di = NUM_ADD(NUM_MUL(new_dr, zi), NUM_MUL(new_di, zr));
            new_dr = NUM_SUB(NUM_MUL(new_dr, zr), dizi);
        }

        const number_t t1 = NUM_SUB(NUM_MUL(new_dr, dr), NUM_MUL(new_di, di));
        const number_t t2 = NUM_ADD(NUM_MUL(new_dr, di), NUM_MUL(new_di, dr));

        new_dr = NUM_ADD(NUM_MUL(t1, NUM_VAR(N)), NUM_CONST(1.0));
        new_di = NUM_MUL(t2, NUM_VAR(N));
    } else {
        const number_t pw = NUM_SUB(NUM_VAR(N), NUM_CONST(1.0));
        const number_t rp = NUM_POW(mag, NUM_DIV(pw, NUM_CONST(2.0)));

        const number_t theta = NUM_ATAN2(zi, zr);
        const number_t angle = NUM_MUL(pw, theta);

        new_dr = NUM_MUL(NUM_VAR(N), NUM_MUL(rp, NUM_COS(angle)));
        new_di = NUM_MUL(NUM_VAR(N), NUM_MUL(rp, NUM_SIN(angle)));

        const number_t t1 = NUM_SUB(NUM_MUL(new_dr, dr), NUM_MUL(new_di, di));
        const number_t t2 = NUM_ADD(NUM_MUL(new_dr, di), NUM_MUL(new_di, dr));

        new_dr = NUM_ADD(t1, NUM_CONST(1.0));
        new_di = t2;
    }
}
