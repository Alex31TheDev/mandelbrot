{
    qd_number_t zr2 = zr * zr, zi2 = zi * zi;
    mag = zr2 + zi2;

    if (mag > bailout_qd) break;

    qd_number_t new_zr = zr, new_zi = zi;
    qd_number_t new_dr = dr, new_di = di;

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
