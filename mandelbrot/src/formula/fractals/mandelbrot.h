#include "formula/FormulaTypes.h"

#define formula_normalPower \
    new_zr = NUM_ADD(NUM_SUB(zr2, zi2), cr); \
    new_zi = NUM_MULADD(NUM_CONST(2.0), NUM_MUL(zr, zi), ci);

#define formula_normalPower4_1 \
    const number_t xyn1 = NUM_MUL(zr1, zi1); \
    const number_t xyn2 = NUM_MUL(zr2, zi2); \
    const number_t xyn3 = NUM_MUL(zr3, zi3); \
    const number_t xyn4 = NUM_MUL(zr4, zi4); \
    \
    const number_t xy1 = NUM_ADD(xyn1, xyn1); \
    const number_t xy2 = NUM_ADD(xyn2, xyn2); \
    const number_t xy3 = NUM_ADD(xyn3, xyn3); \
    const number_t xy4 = NUM_ADD(xyn4, xyn4); \
    \
    const number_t xn1 = NUM_SUB(zr21, zi21); \
    const number_t xn2 = NUM_SUB(zr22, zi22); \
    const number_t xn3 = NUM_SUB(zr23, zi23); \
    const number_t xn4 = NUM_SUB(zr24, zi24); \
    \
    new_zi1 = NUM_ADD(xy1, ci1); \
    new_zi2 = NUM_ADD(xy2, ci2); \
    new_zi3 = NUM_ADD(xy3, ci3); \
    new_zi4 = NUM_ADD(xy4, ci4); \
    \
    new_zr1 = NUM_ADD(xn1, cr1); \
    new_zr2 = NUM_ADD(xn2, cr2); \
    new_zr3 = NUM_ADD(xn3, cr3); \
    new_zr4 = NUM_ADD(xn4, cr4);

#define formula_normalPower4_2 \
    const number_t zr1_sq1 = NUM_MUL(zr1, zr1); \
    const number_t zr2_sq1 = NUM_MUL(zr2, zr2); \
    const number_t zr3_sq1 = NUM_MUL(zr3, zr3); \
    const number_t zr4_sq1 = NUM_MUL(zr4, zr4); \
    \
    const number_t zi1_sq1 = NUM_MUL(zi1, zi1); \
    const number_t zi2_sq1 = NUM_MUL(zi2, zi2); \
    const number_t zi3_sq1 = NUM_MUL(zi3, zi3); \
    const number_t zi4_sq1 = NUM_MUL(zi4, zi4); \
    \
    const number_t xyn11 = NUM_MUL(zr1, zi1); \
    const number_t xyn12 = NUM_MUL(zr2, zi2); \
    const number_t xyn13 = NUM_MUL(zr3, zi3); \
    const number_t xyn14 = NUM_MUL(zr4, zi4); \
    \
    const number_t xy11 = NUM_ADD(xyn11, xyn11); \
    const number_t xy12 = NUM_ADD(xyn12, xyn12); \
    const number_t xy13 = NUM_ADD(xyn13, xyn13); \
    const number_t xy14 = NUM_ADD(xyn14, xyn14); \
    \
    const number_t xn11 = NUM_SUB(zr1_sq1, zi1_sq1); \
    const number_t xn12 = NUM_SUB(zr2_sq1, zi2_sq1); \
    const number_t xn13 = NUM_SUB(zr3_sq1, zi3_sq1); \
    const number_t xn14 = NUM_SUB(zr4_sq1, zi4_sq1); \
    \
    zr1_step1 = NUM_ADD(xn11, cr1); \
    zr2_step1 = NUM_ADD(xn12, cr2); \
    zr3_step1 = NUM_ADD(xn13, cr3); \
    zr4_step1 = NUM_ADD(xn14, cr4); \
    \
    zi1_step1 = NUM_ADD(xy11, ci1); \
    zi2_step1 = NUM_ADD(xy12, ci2); \
    zi3_step1 = NUM_ADD(xy13, ci3); \
    zi4_step1 = NUM_ADD(xy14, ci4); \
    \
    mag1_step1 = NUM_ADDXX(zr1_step1, zr1_step1, zi1_step1, zi1_step1); \
    mag2_step1 = NUM_ADDXX(zr2_step1, zr2_step1, zi2_step1, zi2_step1); \
    mag3_step1 = NUM_ADDXX(zr3_step1, zr3_step1, zi3_step1, zi3_step1); \
    mag4_step1 = NUM_ADDXX(zr4_step1, zr4_step1, zi4_step1, zi4_step1); \
    \
    const number_t zr1_sq2 = NUM_MUL(zr1_step1, zr1_step1); \
    const number_t zr2_sq2 = NUM_MUL(zr2_step1, zr2_step1); \
    const number_t zr3_sq2 = NUM_MUL(zr3_step1, zr3_step1); \
    const number_t zr4_sq2 = NUM_MUL(zr4_step1, zr4_step1); \
    \
    const number_t zi1_sq2 = NUM_MUL(zi1_step1, zi1_step1); \
    const number_t zi2_sq2 = NUM_MUL(zi2_step1, zi2_step1); \
    const number_t zi3_sq2 = NUM_MUL(zi3_step1, zi3_step1); \
    const number_t zi4_sq2 = NUM_MUL(zi4_step1, zi4_step1); \
    \
    const number_t xyn21 = NUM_MUL(zr1_step1, zi1_step1); \
    const number_t xyn22 = NUM_MUL(zr2_step1, zi2_step1); \
    const number_t xyn23 = NUM_MUL(zr3_step1, zi3_step1); \
    const number_t xyn24 = NUM_MUL(zr4_step1, zi4_step1); \
    \
    const number_t xy21 = NUM_ADD(xyn21, xyn21); \
    const number_t xy22 = NUM_ADD(xyn22, xyn22); \
    const number_t xy23 = NUM_ADD(xyn23, xyn23); \
    const number_t xy24 = NUM_ADD(xyn24, xyn24); \
    \
    const number_t xn21 = NUM_SUB(zr1_sq2, zi1_sq2); \
    const number_t xn22 = NUM_SUB(zr2_sq2, zi2_sq2); \
    const number_t xn23 = NUM_SUB(zr3_sq2, zi3_sq2); \
    const number_t xn24 = NUM_SUB(zr4_sq2, zi4_sq2); \
    \
    zr1_step2 = NUM_ADD(xn21, cr1); \
    zr2_step2 = NUM_ADD(xn22, cr2); \
    zr3_step2 = NUM_ADD(xn23, cr3); \
    zr4_step2 = NUM_ADD(xn24, cr4); \
    \
    zi1_step2 = NUM_ADD(xy21, ci1); \
    zi2_step2 = NUM_ADD(xy22, ci2); \
    zi3_step2 = NUM_ADD(xy23, ci3); \
    zi4_step2 = NUM_ADD(xy24, ci4); \
    \
    mag1_step2 = NUM_ADDXX(zr1_step2, zr1_step2, zi1_step2, zi1_step2); \
    mag2_step2 = NUM_ADDXX(zr2_step2, zr2_step2, zi2_step2, zi2_step2); \
    mag3_step2 = NUM_ADDXX(zr3_step2, zr3_step2, zi3_step2, zi3_step2); \
    mag4_step2 = NUM_ADDXX(zr4_step2, zr4_step2, zi4_step2, zi4_step2);

#define formula_wholePower \
    number_t f_pzr = zr; \
    number_t f_pzi = zi; \
    \
    number_t f_pzr2 = zr2; \
    number_t f_pzi2 = zi2; \
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

#define derivative_normalPower4_1 \
    const number_t t11 = NUM_SUBXX(zr1, dr1, zi1, di1); \
    const number_t t21 = NUM_SUBXX(zr2, dr2, zi2, di2); \
    const number_t t31 = NUM_SUBXX(zr3, dr3, zi3, di3); \
    const number_t t41 = NUM_SUBXX(zr4, dr4, zi4, di4); \
    \
    const number_t t12 = NUM_ADDXX(zr1, di1, zi1, dr1); \
    const number_t t22 = NUM_ADDXX(zr2, di2, zi2, dr2); \
    const number_t t32 = NUM_ADDXX(zr3, di3, zi3, dr3); \
    const number_t t42 = NUM_ADDXX(zr4, di4, zi4, dr4); \
    \
    new_dr1 = NUM_MULADD(t11, NUM_CONST(2.0), NUM_CONST(1.0)); \
    new_dr2 = NUM_MULADD(t21, NUM_CONST(2.0), NUM_CONST(1.0)); \
    new_dr3 = NUM_MULADD(t31, NUM_CONST(2.0), NUM_CONST(1.0)); \
    new_dr4 = NUM_MULADD(t41, NUM_CONST(2.0), NUM_CONST(1.0)); \
    new_di1 = NUM_DOUBLE(t12); \
    new_di2 = NUM_DOUBLE(t22); \
    new_di3 = NUM_DOUBLE(t32); \
    new_di4 = NUM_DOUBLE(t42);

#define derivative_normalPower4_2 \
    const number_t t11_1 = NUM_SUBXX(zr1, dr1, zi1, di1); \
    const number_t t11_2 = NUM_SUBXX(zr2, dr2, zi2, di2); \
    const number_t t11_3 = NUM_SUBXX(zr3, dr3, zi3, di3); \
    const number_t t11_4 = NUM_SUBXX(zr4, dr4, zi4, di4); \
    \
    const number_t t12_1 = NUM_ADDXX(zr1, di1, zi1, dr1); \
    const number_t t12_2 = NUM_ADDXX(zr2, di2, zi2, dr2); \
    const number_t t12_3 = NUM_ADDXX(zr3, di3, zi3, dr3); \
    const number_t t12_4 = NUM_ADDXX(zr4, di4, zi4, dr4); \
    \
    dr1_step1 = NUM_MULADD(t11_1, NUM_CONST(2.0), NUM_CONST(1.0)); \
    dr2_step1 = NUM_MULADD(t11_2, NUM_CONST(2.0), NUM_CONST(1.0)); \
    dr3_step1 = NUM_MULADD(t11_3, NUM_CONST(2.0), NUM_CONST(1.0)); \
    dr4_step1 = NUM_MULADD(t11_4, NUM_CONST(2.0), NUM_CONST(1.0)); \
    \
    di1_step1 = NUM_DOUBLE(t12_1); \
    di2_step1 = NUM_DOUBLE(t12_2); \
    di3_step1 = NUM_DOUBLE(t12_3); \
    di4_step1 = NUM_DOUBLE(t12_4); \
    \
    const number_t t21_1 = NUM_SUBXX(zr1_step1, dr1_step1, zi1_step1, di1_step1); \
    const number_t t21_2 = NUM_SUBXX(zr2_step1, dr2_step1, zi2_step1, di2_step1); \
    const number_t t21_3 = NUM_SUBXX(zr3_step1, dr3_step1, zi3_step1, di3_step1); \
    const number_t t21_4 = NUM_SUBXX(zr4_step1, dr4_step1, zi4_step1, di4_step1); \
    \
    const number_t t22_1 = NUM_ADDXX(zr1_step1, di1_step1, zi1_step1, dr1_step1); \
    const number_t t22_2 = NUM_ADDXX(zr2_step1, di2_step1, zi2_step1, dr2_step1); \
    const number_t t22_3 = NUM_ADDXX(zr3_step1, di3_step1, zi3_step1, dr3_step1); \
    const number_t t22_4 = NUM_ADDXX(zr4_step1, di4_step1, zi4_step1, dr4_step1); \
    \
    dr1_step2 = NUM_MULADD(t21_1, NUM_CONST(2.0), NUM_CONST(1.0)); \
    dr2_step2 = NUM_MULADD(t21_2, NUM_CONST(2.0), NUM_CONST(1.0)); \
    dr3_step2 = NUM_MULADD(t21_3, NUM_CONST(2.0), NUM_CONST(1.0)); \
    dr4_step2 = NUM_MULADD(t21_4, NUM_CONST(2.0), NUM_CONST(1.0)); \
    \
    di1_step2 = NUM_DOUBLE(t22_1); \
    di2_step2 = NUM_DOUBLE(t22_2); \
    di3_step2 = NUM_DOUBLE(t22_3); \
    di4_step2 = NUM_DOUBLE(t22_4);

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

#ifdef USE_MPFR

FORCE_INLINE void normalFormula_mandelbrot_mp(
    mpfr_srcptr cr, mpfr_srcptr ci,
    MPFRScratch &s
) {
    mpfr_sub(s.t[0], s.zr2, s.zi2, MPFRTypes::ROUNDING);
    mpfr_add(s.t[0], s.t[0], cr, MPFRTypes::ROUNDING);

    mpfr_mul(s.t[1], s.zr, s.zi, MPFRTypes::ROUNDING);
    mpfr_mul_2ui(s.t[1], s.t[1], 1, MPFRTypes::ROUNDING);
    mpfr_add(s.t[1], s.t[1], ci, MPFRTypes::ROUNDING);

    mpfr_set(s.zr, s.t[0], MPFRTypes::ROUNDING);
    mpfr_set(s.zi, s.t[1], MPFRTypes::ROUNDING);
}

FORCE_INLINE void wholeFormula_mandelbrot_mp(
    mpfr_srcptr cr, mpfr_srcptr ci,
    MPFRScratch &s
) {
    wholePowerCore_mp(s.zr, s.zi, static_cast<int>(ScalarGlobals::N), s);
    mpfr_add(s.zr, s.zr, cr, MPFRTypes::ROUNDING);
    mpfr_add(s.zi, s.zi, ci, MPFRTypes::ROUNDING);
}

FORCE_INLINE void fractionalFormula_mandelbrot_mp(
    mpfr_srcptr cr, mpfr_srcptr ci,
    MPFRScratch &s
) {
    fractionalPowerCore_mp(s.zr, s.zi, s.nVal, s.halfN, s);
    mpfr_add(s.zr, s.zr, cr, MPFRTypes::ROUNDING);
    mpfr_add(s.zi, s.zi, ci, MPFRTypes::ROUNDING);
}

FORCE_INLINE void normalDerivative_mandelbrot_mp(MPFRScratch &s) {
    mpfr_mul(s.t[0], s.zr, s.dr, MPFRTypes::ROUNDING);
    mpfr_mul(s.t[1], s.zi, s.di, MPFRTypes::ROUNDING);
    mpfr_sub(s.t[2], s.t[0], s.t[1], MPFRTypes::ROUNDING);

    mpfr_mul(s.t[3], s.zr, s.di, MPFRTypes::ROUNDING);
    mpfr_mul(s.t[4], s.zi, s.dr, MPFRTypes::ROUNDING);
    mpfr_add(s.t[6], s.t[3], s.t[4], MPFRTypes::ROUNDING);

    mpfr_mul_2ui(s.t[7], s.t[2], 1, MPFRTypes::ROUNDING);
    mpfr_add_ui(s.dr, s.t[7], 1, MPFRTypes::ROUNDING);

    mpfr_mul_2ui(s.t[8], s.t[6], 1, MPFRTypes::ROUNDING);
    mpfr_set(s.di, s.t[8], MPFRTypes::ROUNDING);
}

FORCE_INLINE void wholeDerivative_mandelbrot_mp(MPFRScratch &s) {
    wholePowerCore_mp(s.zr, s.zi, static_cast<int>(ScalarGlobals::N) - 1, s);
    mpfr_set(s.t[0], s.zr, MPFRTypes::ROUNDING);
    mpfr_set(s.t[1], s.zi, MPFRTypes::ROUNDING);

    mpfr_mul(s.t[2], s.t[0], s.dr, MPFRTypes::ROUNDING);
    mpfr_mul(s.t[3], s.t[1], s.di, MPFRTypes::ROUNDING);
    mpfr_sub(s.t[4], s.t[2], s.t[3], MPFRTypes::ROUNDING);

    mpfr_mul(s.t[2], s.t[0], s.di, MPFRTypes::ROUNDING);
    mpfr_mul(s.t[3], s.t[1], s.dr, MPFRTypes::ROUNDING);
    mpfr_add(s.t[5], s.t[2], s.t[3], MPFRTypes::ROUNDING);

    mpfr_mul(s.t[6], s.t[4], s.nVal, MPFRTypes::ROUNDING);
    mpfr_add_ui(s.dr, s.t[6], 1, MPFRTypes::ROUNDING);
    mpfr_mul(s.di, s.t[5], s.nVal, MPFRTypes::ROUNDING);
}

FORCE_INLINE void fractionalDerivative_mandelbrot_mp(MPFRScratch &s) {
    fractionalPowerCore_mp(s.zr, s.zi, s.nMinus1, s.halfNMinus1, s);
    mpfr_mul(s.t[6], s.zr, s.nVal, MPFRTypes::ROUNDING);
    mpfr_mul(s.t[7], s.zi, s.nVal, MPFRTypes::ROUNDING);

    mpfr_mul(s.t[8], s.t[6], s.dr, MPFRTypes::ROUNDING);
    mpfr_mul(s.t[9], s.t[7], s.di, MPFRTypes::ROUNDING);
    mpfr_sub(s.t[10], s.t[8], s.t[9], MPFRTypes::ROUNDING);

    mpfr_mul(s.t[8], s.t[6], s.di, MPFRTypes::ROUNDING);
    mpfr_mul(s.t[9], s.t[7], s.dr, MPFRTypes::ROUNDING);
    mpfr_add(s.t[11], s.t[8], s.t[9], MPFRTypes::ROUNDING);

    mpfr_add_ui(s.dr, s.t[10], 1, MPFRTypes::ROUNDING);
    mpfr_set(s.di, s.t[11], MPFRTypes::ROUNDING);
}

#endif
