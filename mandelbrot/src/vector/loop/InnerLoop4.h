{
    const int pairCount = count / 2;

    for (int pairIdx = 0; pairIdx < pairCount; ++pairIdx) {
        const simd_full_mask_t anyStartActive = SIMD_OR_MASK_F(
            SIMD_OR_MASK_F(active1, active2),
            SIMD_OR_MASK_F(active3, active4)
        );

        if (SIMD_MASK_ALLZERO_F(anyStartActive)) {
            break;
        }

        simd_full_t zr1_step1, zi1_step1, mag1_step1;
        simd_full_t zr2_step1, zi2_step1, mag2_step1;
        simd_full_t zr3_step1, zi3_step1, mag3_step1;
        simd_full_t zr4_step1, zi4_step1, mag4_step1;

        simd_full_t zr1_step2, zi1_step2, mag1_step2;
        simd_full_t zr2_step2, zi2_step2, mag2_step2;
        simd_full_t zr3_step2, zi3_step2, mag3_step2;
        simd_full_t zr4_step2, zi4_step2, mag4_step2;

#ifdef _USE_DERIVATIVE
        simd_full_t dr1_step1, di1_step1, dr1_step2, di1_step2;
        simd_full_t dr2_step1, di2_step1, dr2_step2, di2_step2;
        simd_full_t dr3_step1, di3_step1, dr3_step2, di3_step2;
        simd_full_t dr4_step1, di4_step1, dr4_step2, di4_step2;
#endif
        _FORMULA4_2;
#ifdef _USE_DERIVATIVE
        _DERIVATIVE4_2;
#endif

        const simd_full_mask_t cmp1_1 = SIMD_CMP_LT_F(mag1_step1, f_bailout_vec);
        const simd_full_mask_t cmp1_2 = SIMD_CMP_LT_F(mag2_step1, f_bailout_vec);
        const simd_full_mask_t cmp1_3 = SIMD_CMP_LT_F(mag3_step1, f_bailout_vec);
        const simd_full_mask_t cmp1_4 = SIMD_CMP_LT_F(mag4_step1, f_bailout_vec);

        const simd_full_mask_t escape1_1 = SIMD_ANDNOT_MASK_F(cmp1_1, active1);
        const simd_full_mask_t escape1_2 = SIMD_ANDNOT_MASK_F(cmp1_2, active2);
        const simd_full_mask_t escape1_3 = SIMD_ANDNOT_MASK_F(cmp1_3, active3);
        const simd_full_mask_t escape1_4 = SIMD_ANDNOT_MASK_F(cmp1_4, active4);

        active1 = SIMD_AND_MASK_F(active1, cmp1_1);
        active2 = SIMD_AND_MASK_F(active2, cmp1_2);
        active3 = SIMD_AND_MASK_F(active3, cmp1_3);
        active4 = SIMD_AND_MASK_F(active4, cmp1_4);

        const simd_full_mask_t pairActive1 = SIMD_OR_MASK_F(escape1_1, active1);
        const simd_full_mask_t pairActive2 = SIMD_OR_MASK_F(escape1_2, active2);
        const simd_full_mask_t pairActive3 = SIMD_OR_MASK_F(escape1_3, active3);
        const simd_full_mask_t pairActive4 = SIMD_OR_MASK_F(escape1_4, active4);

        iter1 = SIMD_ADD_MASK_F(iter1, SIMD_ONE_F, pairActive1);
        iter2 = SIMD_ADD_MASK_F(iter2, SIMD_ONE_F, pairActive2);
        iter3 = SIMD_ADD_MASK_F(iter3, SIMD_ONE_F, pairActive3);
        iter4 = SIMD_ADD_MASK_F(iter4, SIMD_ONE_F, pairActive4);

        iter1 = SIMD_ADD_MASK_F(iter1, SIMD_ONE_F, active1);
        iter2 = SIMD_ADD_MASK_F(iter2, SIMD_ONE_F, active2);
        iter3 = SIMD_ADD_MASK_F(iter3, SIMD_ONE_F, active3);
        iter4 = SIMD_ADD_MASK_F(iter4, SIMD_ONE_F, active4);

        const simd_full_mask_t cmp2_1 = SIMD_CMP_LT_F(mag1_step2, f_bailout_vec);
        const simd_full_mask_t cmp2_2 = SIMD_CMP_LT_F(mag2_step2, f_bailout_vec);
        const simd_full_mask_t cmp2_3 = SIMD_CMP_LT_F(mag3_step2, f_bailout_vec);
        const simd_full_mask_t cmp2_4 = SIMD_CMP_LT_F(mag4_step2, f_bailout_vec);

        active1 = SIMD_AND_MASK_F(active1, cmp2_1);
        active2 = SIMD_AND_MASK_F(active2, cmp2_2);
        active3 = SIMD_AND_MASK_F(active3, cmp2_3);
        active4 = SIMD_AND_MASK_F(active4, cmp2_4);

        zr1 = SIMD_BLEND_F(zr1, zr1_step2, pairActive1);
        zr2 = SIMD_BLEND_F(zr2, zr2_step2, pairActive2);
        zr3 = SIMD_BLEND_F(zr3, zr3_step2, pairActive3);
        zr4 = SIMD_BLEND_F(zr4, zr4_step2, pairActive4);
        zi1 = SIMD_BLEND_F(zi1, zi1_step2, pairActive1);
        zi2 = SIMD_BLEND_F(zi2, zi2_step2, pairActive2);
        zi3 = SIMD_BLEND_F(zi3, zi3_step2, pairActive3);
        zi4 = SIMD_BLEND_F(zi4, zi4_step2, pairActive4);

        zr1 = SIMD_BLEND_F(zr1, zr1_step1, escape1_1);
        zr2 = SIMD_BLEND_F(zr2, zr2_step1, escape1_2);
        zr3 = SIMD_BLEND_F(zr3, zr3_step1, escape1_3);
        zr4 = SIMD_BLEND_F(zr4, zr4_step1, escape1_4);
        zi1 = SIMD_BLEND_F(zi1, zi1_step1, escape1_1);
        zi2 = SIMD_BLEND_F(zi2, zi2_step1, escape1_2);
        zi3 = SIMD_BLEND_F(zi3, zi3_step1, escape1_3);
        zi4 = SIMD_BLEND_F(zi4, zi4_step1, escape1_4);

        mag1 = SIMD_BLEND_F(mag1, mag1_step2, pairActive1);
        mag2 = SIMD_BLEND_F(mag2, mag2_step2, pairActive2);
        mag3 = SIMD_BLEND_F(mag3, mag3_step2, pairActive3);
        mag4 = SIMD_BLEND_F(mag4, mag4_step2, pairActive4);

        mag1 = SIMD_BLEND_F(mag1, mag1_step1, escape1_1);
        mag2 = SIMD_BLEND_F(mag2, mag2_step1, escape1_2);
        mag3 = SIMD_BLEND_F(mag3, mag3_step1, escape1_3);
        mag4 = SIMD_BLEND_F(mag4, mag4_step1, escape1_4);

#ifdef _USE_DERIVATIVE
        dr1 = SIMD_BLEND_F(dr1, dr1_step2, pairActive1);
        dr2 = SIMD_BLEND_F(dr2, dr2_step2, pairActive2);
        dr3 = SIMD_BLEND_F(dr3, dr3_step2, pairActive3);
        dr4 = SIMD_BLEND_F(dr4, dr4_step2, pairActive4);
        di1 = SIMD_BLEND_F(di1, di1_step2, pairActive1);
        di2 = SIMD_BLEND_F(di2, di2_step2, pairActive2);
        di3 = SIMD_BLEND_F(di3, di3_step2, pairActive3);
        di4 = SIMD_BLEND_F(di4, di4_step2, pairActive4);

        dr1 = SIMD_BLEND_F(dr1, dr1_step1, escape1_1);
        dr2 = SIMD_BLEND_F(dr2, dr2_step1, escape1_2);
        dr3 = SIMD_BLEND_F(dr3, dr3_step1, escape1_3);
        dr4 = SIMD_BLEND_F(dr4, dr4_step1, escape1_4);
        di1 = SIMD_BLEND_F(di1, di1_step1, escape1_1);
        di2 = SIMD_BLEND_F(di2, di2_step1, escape1_2);
        di3 = SIMD_BLEND_F(di3, di3_step1, escape1_3);
        di4 = SIMD_BLEND_F(di4, di4_step1, escape1_4);
#endif
    }

    if ((count & 1) != 0) {
        const simd_full_mask_t anyActivePreTail = SIMD_OR_MASK_F(
            SIMD_OR_MASK_F(active1, active2),
            SIMD_OR_MASK_F(active3, active4)
        );

        if (!SIMD_MASK_ALLZERO_F(anyActivePreTail)) {
            zr21 = SIMD_SQUARE_F(zr1);
            zr22 = SIMD_SQUARE_F(zr2);
            zr23 = SIMD_SQUARE_F(zr3);
            zr24 = SIMD_SQUARE_F(zr4);
            zi21 = SIMD_SQUARE_F(zi1);
            zi22 = SIMD_SQUARE_F(zi2);
            zi23 = SIMD_SQUARE_F(zi3);
            zi24 = SIMD_SQUARE_F(zi4);

            mag1 = SIMD_ADD_F(zr21, zi21);
            mag2 = SIMD_ADD_F(zr22, zi22);
            mag3 = SIMD_ADD_F(zr23, zi23);
            mag4 = SIMD_ADD_F(zr24, zi24);

            active1 = SIMD_AND_MASK_F(active1, SIMD_CMP_LT_F(mag1, f_bailout_vec));
            active2 = SIMD_AND_MASK_F(active2, SIMD_CMP_LT_F(mag2, f_bailout_vec));
            active3 = SIMD_AND_MASK_F(active3, SIMD_CMP_LT_F(mag3, f_bailout_vec));
            active4 = SIMD_AND_MASK_F(active4, SIMD_CMP_LT_F(mag4, f_bailout_vec));

            const simd_full_mask_t anyActiveTail = SIMD_OR_MASK_F(
                SIMD_OR_MASK_F(active1, active2),
                SIMD_OR_MASK_F(active3, active4)
            );

            if (!SIMD_MASK_ALLZERO_F(anyActiveTail)) {
#ifdef _USE_DERIVATIVE
                _DERIVATIVE4_1;
#endif
                _FORMULA4_1;

                iter1 = SIMD_ADD_MASK_F(iter1, SIMD_ONE_F, active1);
                iter2 = SIMD_ADD_MASK_F(iter2, SIMD_ONE_F, active2);
                iter3 = SIMD_ADD_MASK_F(iter3, SIMD_ONE_F, active3);
                iter4 = SIMD_ADD_MASK_F(iter4, SIMD_ONE_F, active4);

#ifdef _USE_DERIVATIVE
                dr1 = SIMD_BLEND_F(dr1, new_dr1, active1);
                dr2 = SIMD_BLEND_F(dr2, new_dr2, active2);
                dr3 = SIMD_BLEND_F(dr3, new_dr3, active3);
                dr4 = SIMD_BLEND_F(dr4, new_dr4, active4);
                di1 = SIMD_BLEND_F(di1, new_di1, active1);
                di2 = SIMD_BLEND_F(di2, new_di2, active2);
                di3 = SIMD_BLEND_F(di3, new_di3, active3);
                di4 = SIMD_BLEND_F(di4, new_di4, active4);
#endif

                zr1 = SIMD_BLEND_F(zr1, new_zr1, active1);
                zr2 = SIMD_BLEND_F(zr2, new_zr2, active2);
                zr3 = SIMD_BLEND_F(zr3, new_zr3, active3);
                zr4 = SIMD_BLEND_F(zr4, new_zr4, active4);
                zi1 = SIMD_BLEND_F(zi1, new_zi1, active1);
                zi2 = SIMD_BLEND_F(zi2, new_zi2, active2);
                zi3 = SIMD_BLEND_F(zi3, new_zi3, active3);
                zi4 = SIMD_BLEND_F(zi4, new_zi4, active4);
            }
        }
    }
}
