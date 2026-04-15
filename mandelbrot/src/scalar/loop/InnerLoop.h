{
    scalar_full_t zr2 = zr * zr;
    scalar_full_t zi2 = zi * zi;
    mag = zr2 + zi2;

    if (mag > BAILOUT) break;

    scalar_full_t new_zr = zr, new_zi = zi;
    scalar_full_t new_dr = dr, new_di = di;

#ifdef _USE_DERIVATIVE
    _DERIVATIVE;
#endif
    _FORMULA;

#ifdef _USE_DERIVATIVE
    dr = new_dr;
    di = new_di;
#endif

    zr = new_zr;
    zi = new_zi;
}