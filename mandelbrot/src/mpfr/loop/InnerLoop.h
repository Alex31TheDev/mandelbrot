{
    mpfr_number_t zr2 = zr * zr;
    mpfr_number_t zi2 = zi * zi;
    mag = zr2 + zi2;

    if (mag > bailout_mp) break;

#ifdef _USE_DERIVATIVE
    _DERIVATIVE;
#endif
    _FORMULA;
}