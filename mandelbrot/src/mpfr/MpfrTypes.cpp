#include "MpfrTypes.h"

static mpfr_t _result_pool[RESULT_SLOTS];
static int _result_cursor = 0;

void init_mpfr_pool() {
    for (int i = 0; i < RESULT_SLOTS; i++) {
        mpfr_init2(_result_pool[i], PRECISION);
    }
}

void clear_mpfr_pool() {
    for (int i = 0; i < RESULT_SLOTS; i++) {
        mpfr_clear(_result_pool[i]);
    }
}

void init_set(mpfr_t &var, double val) {
    mpfr_init2(var, PRECISION);
    mpfr_set_d(var, val, ROUNDING);
}

void create_copy(mpfr_t &dest, const mpfr_t *src) {
    mpfr_init2(dest, PRECISION);
    mpfr_set(dest, *src, ROUNDING);
}

static mpfr_t *_next_result_slot() {
    mpfr_t *res = &_result_pool[_result_cursor];
    _result_cursor = (_result_cursor + 1) % RESULT_SLOTS;
    return res;
}

mpfr_t *init_set_res(double val) {
    mpfr_t *res = _next_result_slot();
    mpfr_set_d(*res, val, ROUNDING);
    return res;
}

mpfr_t *add(const mpfr_t *a, const mpfr_t *b) {
    mpfr_t *res = _next_result_slot();
    mpfr_add(*res, *a, *b, ROUNDING);
    return res;
}

mpfr_t *sub(const mpfr_t *a, const mpfr_t *b) {
    mpfr_t *res = _next_result_slot();
    mpfr_sub(*res, *a, *b, ROUNDING);
    return res;
}

mpfr_t *mul(const mpfr_t *a, const mpfr_t *b) {
    mpfr_t *res = _next_result_slot();
    mpfr_mul(*res, *a, *b, ROUNDING);
    return res;
}

mpfr_t *div(const mpfr_t *a, const mpfr_t *b) {
    mpfr_t *res = _next_result_slot();
    mpfr_div(*res, *a, *b, ROUNDING);
    return res;
}

mpfr_t *neg(const mpfr_t *a) {
    mpfr_t *res = _next_result_slot();
    mpfr_neg(*res, *a, ROUNDING);
    return res;
}

mpfr_t *sqrt(const mpfr_t *a) {
    mpfr_t *res = _next_result_slot();
    mpfr_sqrt(*res, *a, ROUNDING);
    return res;
}

mpfr_t *pow(const mpfr_t *a, const mpfr_t *b) {
    mpfr_t *res = _next_result_slot();
    mpfr_pow(*res, *a, *b, ROUNDING);
    return res;
}