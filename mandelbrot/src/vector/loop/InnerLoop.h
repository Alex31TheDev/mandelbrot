{
    zr2 = SIMD_SQUARE_F(zr);
    zi2 = SIMD_SQUARE_F(zi);
    mag = SIMD_ADD_F(zr2, zi2);

    active = SIMD_AND_MASK_F(active, SIMD_CMP_LT_F(mag, f_bailout_vec));
    if (SIMD_MASK_ALLZERO_F(active)) break;

#ifdef _USE_DERIVATIVE
    _DERIVATIVE;
#endif
    _FORMULA;

    iter = SIMD_ADD_MASK_F(iter, SIMD_ONE_F, active);

#ifdef _USE_DERIVATIVE
    dr = SIMD_BLEND_F(dr, new_dr, active);
    di = SIMD_BLEND_F(di, new_di, active);
#endif

    zr = SIMD_BLEND_F(zr, new_zr, active);
    zi = SIMD_BLEND_F(zi, new_zi, active);
}