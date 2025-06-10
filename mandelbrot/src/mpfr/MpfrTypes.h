#pragma once

#include <mpfr.h>

constexpr mpfr_prec_t PRECISION = 256;
constexpr mpfr_rnd_t ROUNDING = MPFR_RNDN;
constexpr int RESULT_SLOTS = 8;

void init_mpfr_pool();
void clear_mpfr_pool();
void init_set(mpfr_t &var, double val);
mpfr_t *init_set_res(double val);
void create_copy(mpfr_t &dest, const mpfr_t *src);

mpfr_t *add(const mpfr_t *a, const mpfr_t *b);
mpfr_t *sub(const mpfr_t *a, const mpfr_t *b);
mpfr_t *mul(const mpfr_t *a, const mpfr_t *b);
mpfr_t *div(const mpfr_t *a, const mpfr_t *b);
mpfr_t *neg(const mpfr_t *a);
mpfr_t *sqrt(const mpfr_t *a);
mpfr_t *pow(const mpfr_t *a, const mpfr_t *b);
