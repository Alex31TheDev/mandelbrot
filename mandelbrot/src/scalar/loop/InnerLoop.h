{
    scalar_full_t zr2 = zr * zr;
    scalar_full_t zi2 = zi * zi;
    mag = zr2 + zi2;

    if (mag > BAILOUT) break;

#ifdef _USE_DERIVATIVE
    _DERIVATIVE;
#endif
    _FORMULA;
}