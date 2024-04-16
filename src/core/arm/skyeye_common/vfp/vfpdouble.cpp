/*
    vfp/vfpdouble.c - ARM VFPv3 emulation unit - SoftFloat double instruction
    Copyright (C) 2003 Skyeye Develop Group
    for help please send mail to <skyeye-developer@lists.gro.clinux.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
 * This code is derived in part from :
 * - Android kernel
 * - John R. Housers softfloat library, which
 * carries the following notice:
 *
 * ===========================================================================
 * This C source file is part of the SoftFloat IEC/IEEE Floating-point
 * Arithmetic Package, Release 2.
 *
 * Written by John R. Hauser.  This work was made possible in part by the
 * International Computer Science Institute, located at Suite 600, 1947 Center
 * Street, Berkeley, California 94704.  Funding was partially provided by the
 * National Science Foundation under grant MIP-9311980.  The original version
 * of this code was written as part of a project to build a fixed-point vector
 * processor in collaboration with the University of California at Berkeley,
 * overseen by Profs. Nelson Morgan and John Wawrzynek.  More information
 * is available through the web page `http://HTTP.CS.Berkeley.EDU/~jhauser/
 * arithmetic/softfloat.html'.
 *
 * THIS SOFTWARE IS DISTRIBUTED AS IS, FOR FREE.  Although reasonable effort
 * has been made to avoid it, THIS SOFTWARE MAY CONTAIN FAULTS THAT WILL AT
 * TIMES RESULT IN INCORRECT BEHAVIOR.  USE OF THIS SOFTWARE IS RESTRICTED TO
 * PERSONS AND ORGANIZATIONS WHO CAN AND WILL TAKE FULL RESPONSIBILITY FOR ANY
 * AND ALL LOSSES, COSTS, OR OTHER PROBLEMS ARISING FROM ITS USE.
 *
 * Derivative works are acceptable, even for commercial purposes, so long as
 * (1) they include prominent notice that the work is derivative, and (2) they
 * include prominent notice akin to these three paragraphs for those parts of
 * this code that are retained.
 * ===========================================================================
 */

#include <algorithm>
#include "common/logging/log.h"
#include "core/arm/skyeye_common/vfp/asm_vfp.h"
#include "core/arm/skyeye_common/vfp/vfp.h"
#include "core/arm/skyeye_common/vfp/vfp_helper.h"

static struct vfp_double vfp_double_default_qnan = {
    2047,
    0,
    VFP_DOUBLE_SIGNIFICAND_QNAN,
};

static void vfp_double_dump(const char* str, struct vfp_double* d) {
    LOG_TRACE(Core_ARM11, "VFP: {}: sign={} exponent={} significand={:016x}", str, d->sign != 0,
              d->exponent, d->significand);
}

static void vfp_double_normalise_denormal(struct vfp_double* vd) {
    int bits = 31 - fls((u32)(vd->significand >> 32));
    if (bits == 31)
        bits = 63 - fls((u32)vd->significand);

    vfp_double_dump("normalise_denormal: in", vd);

    if (bits) {
        vd->exponent -= bits - 1;
        vd->significand <<= bits;
    }

    vfp_double_dump("normalise_denormal: out", vd);
}

u32 vfp_double_normaliseround(ARMul_State* state, int dd, struct vfp_double* vd, u32 fpscr,
                              u32 exceptions, const char* func) {
    u64 significand, incr;
    int exponent, shift, underflow;
    u32 rmode;

    vfp_double_dump("pack: in", vd);

    /*
     * Infinities and NaNs are a special case.
     */
    if (vd->exponent == 2047 && (vd->significand == 0 || exceptions))
        goto pack;

    /*
     * Special-case zero.
     */
    if (vd->significand == 0) {
        vd->exponent = 0;
        goto pack;
    }

    exponent = vd->exponent;
    significand = vd->significand;

    shift = 32 - fls((u32)(significand >> 32));
    if (shift == 32)
        shift = 64 - fls((u32)significand);
    if (shift) {
        exponent -= shift;
        significand <<= shift;
    }

#if 1
    vd->exponent = exponent;
    vd->significand = significand;
    vfp_double_dump("pack: normalised", vd);
#endif

    /*
     * Tiny number?
     */
    underflow = exponent < 0;
    if (underflow) {
        significand = vfp_shiftright64jamming(significand, -exponent);
        exponent = 0;
#if 1
        vd->exponent = exponent;
        vd->significand = significand;
        vfp_double_dump("pack: tiny number", vd);
#endif
        if (!(significand & ((1ULL << (VFP_DOUBLE_LOW_BITS + 1)) - 1)))
            underflow = 0;

        int type = vfp_double_type(vd);

        if ((type & VFP_DENORMAL) && (fpscr & FPSCR_FLUSH_TO_ZERO)) {
            // Flush denormal to positive 0
            significand = 0;

            vd->sign = 0;
            vd->significand = significand;

            underflow = 0;
            exceptions |= FPSCR_UFC;
        }
    }

    /*
     * Select rounding increment.
     */
    incr = 0;
    rmode = fpscr & FPSCR_RMODE_MASK;

    if (rmode == FPSCR_ROUND_NEAREST) {
        incr = 1ULL << VFP_DOUBLE_LOW_BITS;
        if ((significand & (1ULL << (VFP_DOUBLE_LOW_BITS + 1))) == 0)
            incr -= 1;
    } else if (rmode == FPSCR_ROUND_TOZERO) {
        incr = 0;
    } else if ((rmode == FPSCR_ROUND_PLUSINF) ^ (vd->sign != 0))
        incr = (1ULL << (VFP_DOUBLE_LOW_BITS + 1)) - 1;

    LOG_TRACE(Core_ARM11, "VFP: rounding increment = 0x{:08x}", incr);

    /*
     * Is our rounding going to overflow?
     */
    if ((significand + incr) < significand) {
        exponent += 1;
        significand = (significand >> 1) | (significand & 1);
        incr >>= 1;
#if 1
        vd->exponent = exponent;
        vd->significand = significand;
        vfp_double_dump("pack: overflow", vd);
#endif
    }

    /*
     * If any of the low bits (which will be shifted out of the
     * number) are non-zero, the result is inexact.
     */
    if (significand & ((1 << (VFP_DOUBLE_LOW_BITS + 1)) - 1))
        exceptions |= FPSCR_IXC;

    /*
     * Do our rounding.
     */
    significand += incr;

    /*
     * Infinity?
     */
    if (exponent >= 2046) {
        exceptions |= FPSCR_OFC | FPSCR_IXC;
        if (incr == 0) {
            vd->exponent = 2045;
            vd->significand = 0x7fffffffffffffffULL;
        } else {
            vd->exponent = 2047; /* infinity */
            vd->significand = 0;
        }
    } else {
        if (significand >> (VFP_DOUBLE_LOW_BITS + 1) == 0)
            exponent = 0;
        if (exponent || significand > 0x8000000000000000ULL)
            underflow = 0;
        if (underflow)
            exceptions |= FPSCR_UFC;
        vd->exponent = exponent;
        vd->significand = significand >> 1;
    }

pack:
    vfp_double_dump("pack: final", vd);
    {
        s64 d = vfp_double_pack(vd);
        LOG_TRACE(Core_ARM11, "VFP: {}: d(d{})={:016x} exceptions={:08x}", func, dd, d, exceptions);
        vfp_put_double(state, d, dd);
    }
    return exceptions;
}

/*
 * Propagate the NaN, setting exceptions if it is signalling.
 * 'n' is always a NaN.  'm' may be a number, NaN or infinity.
 */
static u32 vfp_propagate_nan(struct vfp_double* vdd, struct vfp_double* vdn, struct vfp_double* vdm,
                             u32 fpscr) {
    struct vfp_double* nan;
    int tn, tm = 0;

    tn = vfp_double_type(vdn);

    if (vdm)
        tm = vfp_double_type(vdm);

    if (fpscr & FPSCR_DEFAULT_NAN)
        /*
         * Default NaN mode - always returns a quiet NaN
         */
        nan = &vfp_double_default_qnan;
    else {
        /*
         * Contemporary mode - select the first signalling
         * NAN, or if neither are signalling, the first
         * quiet NAN.
         */
        if (tn == VFP_SNAN || (tm != VFP_SNAN && tn == VFP_QNAN))
            nan = vdn;
        else
            nan = vdm;
        /*
         * Make the NaN quiet.
         */
        nan->significand |= VFP_DOUBLE_SIGNIFICAND_QNAN;
    }

    *vdd = *nan;

    /*
     * If one was a signalling NAN, raise invalid operation.
     */
    return tn == VFP_SNAN || tm == VFP_SNAN ? FPSCR_IOC : VFP_NAN_FLAG;
}

/*
 * Extended operations
 */
static u32 vfp_double_fabs(ARMul_State* state, int dd, int unused, int dm, u32 fpscr) {
    LOG_TRACE(Core_ARM11, "In {}", __FUNCTION__);
    vfp_put_double(state, vfp_double_packed_abs(vfp_get_double(state, dm)), dd);
    return 0;
}

static u32 vfp_double_fcpy(ARMul_State* state, int dd, int unused, int dm, u32 fpscr) {
    LOG_TRACE(Core_ARM11, "In {}", __FUNCTION__);
    vfp_put_double(state, vfp_get_double(state, dm), dd);
    return 0;
}

static u32 vfp_double_fneg(ARMul_State* state, int dd, int unused, int dm, u32 fpscr) {
    LOG_TRACE(Core_ARM11, "In {}", __FUNCTION__);
    vfp_put_double(state, vfp_double_packed_negate(vfp_get_double(state, dm)), dd);
    return 0;
}

static u32 vfp_double_fsqrt(ARMul_State* state, int dd, int unused, int dm, u32 fpscr) {
    LOG_TRACE(Core_ARM11, "In {}", __FUNCTION__);
    vfp_double vdm, vdd, *vdp;
    int ret, tm;
    u32 exceptions = 0;

    exceptions |= vfp_double_unpack(&vdm, vfp_get_double(state, dm), fpscr);

    tm = vfp_double_type(&vdm);
    if (tm & (VFP_NAN | VFP_INFINITY)) {
        vdp = &vdd;

        if (tm & VFP_NAN)
            ret = vfp_propagate_nan(vdp, &vdm, nullptr, fpscr);
        else if (vdm.sign == 0) {
        sqrt_copy:
            vdp = &vdm;
            ret = 0;
        } else {
        sqrt_invalid:
            vdp = &vfp_double_default_qnan;
            ret = FPSCR_IOC;
        }
        vfp_put_double(state, vfp_double_pack(vdp), dd);
        return ret;
    }

    /*
     * sqrt(+/- 0) == +/- 0
     */
    if (tm & VFP_ZERO)
        goto sqrt_copy;

    /*
     * Normalise a denormalised number
     */
    if (tm & VFP_DENORMAL)
        vfp_double_normalise_denormal(&vdm);

    /*
     * sqrt(<0) = invalid
     */
    if (vdm.sign)
        goto sqrt_invalid;

    vfp_double_dump("sqrt", &vdm);

    /*
     * Estimate the square root.
     */
    vdd.sign = 0;
    vdd.exponent = ((vdm.exponent - 1023) >> 1) + 1023;
    vdd.significand = (u64)vfp_estimate_sqrt_significand(vdm.exponent, vdm.significand >> 32) << 31;

    vfp_double_dump("sqrt estimate1", &vdd);

    vdm.significand >>= 1 + (vdm.exponent & 1);
    vdd.significand += 2 + vfp_estimate_div128to64(vdm.significand, 0, vdd.significand);

    vfp_double_dump("sqrt estimate2", &vdd);

    /*
     * And now adjust.
     */
    if ((vdd.significand & VFP_DOUBLE_LOW_BITS_MASK) <= 5) {
        if (vdd.significand < 2) {
            vdd.significand = ~0ULL;
        } else {
            u64 termh, terml, remh, reml;
            vdm.significand <<= 2;
            mul64to128(&termh, &terml, vdd.significand, vdd.significand);
            sub128(&remh, &reml, vdm.significand, 0, termh, terml);
            while ((s64)remh < 0) {
                vdd.significand -= 1;
                shift64left(&termh, &terml, vdd.significand);
                terml |= 1;
                add128(&remh, &reml, remh, reml, termh, terml);
            }
            vdd.significand |= (remh | reml) != 0;
        }
    }
    vdd.significand = vfp_shiftright64jamming(vdd.significand, 1);

    exceptions |= vfp_double_normaliseround(state, dd, &vdd, fpscr, 0, "fsqrt");

    return exceptions;
}

/*
 * Equal	:= ZC
 * Less than	:= N
 * Greater than	:= C
 * Unordered	:= CV
 */
static u32 vfp_compare(ARMul_State* state, int dd, int signal_on_qnan, s64 m, u32 fpscr) {
    s64 d;
    u32 ret = 0;

    LOG_TRACE(Core_ARM11, "In {}, state=0x{}, fpscr=0x{:x}", __FUNCTION__, fmt::ptr(state), fpscr);
    if (vfp_double_packed_exponent(m) == 2047 && vfp_double_packed_mantissa(m)) {
        ret |= FPSCR_CFLAG | FPSCR_VFLAG;
        if (signal_on_qnan ||
            !(vfp_double_packed_mantissa(m) & (1ULL << (VFP_DOUBLE_MANTISSA_BITS - 1))))
            /*
             * Signalling NaN, or signalling on quiet NaN
             */
            ret |= FPSCR_IOC;
    }

    d = vfp_get_double(state, dd);
    if (vfp_double_packed_exponent(d) == 2047 && vfp_double_packed_mantissa(d)) {
        ret |= FPSCR_CFLAG | FPSCR_VFLAG;
        if (signal_on_qnan ||
            !(vfp_double_packed_mantissa(d) & (1ULL << (VFP_DOUBLE_MANTISSA_BITS - 1))))
            /*
             * Signalling NaN, or signalling on quiet NaN
             */
            ret |= FPSCR_IOC;
    }

    if (ret == 0) {
        // printf("In %s, d=%lld, m =%lld\n ", __FUNCTION__, d, m);
        if (d == m || vfp_double_packed_abs(d | m) == 0) {
            /*
             * equal
             */
            ret |= FPSCR_ZFLAG | FPSCR_CFLAG;
            // printf("In %s,1 ret=0x%x\n", __FUNCTION__, ret);
        } else if (vfp_double_packed_sign(d ^ m)) {
            /*
             * different signs
             */
            if (vfp_double_packed_sign(d))
                /*
                 * d is negative, so d < m
                 */
                ret |= FPSCR_NFLAG;
            else
                /*
                 * d is positive, so d > m
                 */
                ret |= FPSCR_CFLAG;
        } else if ((vfp_double_packed_sign(d) != 0) ^ (d < m)) {
            /*
             * d < m
             */
            ret |= FPSCR_NFLAG;
        } else if ((vfp_double_packed_sign(d) != 0) ^ (d > m)) {
            /*
             * d > m
             */
            ret |= FPSCR_CFLAG;
        }
    }
    LOG_TRACE(Core_ARM11, "In {}, state=0x{}, ret=0x{:x}", __FUNCTION__, fmt::ptr(state), ret);

    return ret;
}

static u32 vfp_double_fcmp(ARMul_State* state, int dd, int unused, int dm, u32 fpscr) {
    LOG_TRACE(Core_ARM11, "In {}", __FUNCTION__);
    return vfp_compare(state, dd, 0, vfp_get_double(state, dm), fpscr);
}

static u32 vfp_double_fcmpe(ARMul_State* state, int dd, int unused, int dm, u32 fpscr) {
    LOG_TRACE(Core_ARM11, "In {}", __FUNCTION__);
    return vfp_compare(state, dd, 1, vfp_get_double(state, dm), fpscr);
}

static u32 vfp_double_fcmpz(ARMul_State* state, int dd, int unused, int dm, u32 fpscr) {
    LOG_TRACE(Core_ARM11, "In {}", __FUNCTION__);
    return vfp_compare(state, dd, 0, 0, fpscr);
}

static u32 vfp_double_fcmpez(ARMul_State* state, int dd, int unused, int dm, u32 fpscr) {
    LOG_TRACE(Core_ARM11, "In {}", __FUNCTION__);
    return vfp_compare(state, dd, 1, 0, fpscr);
}

static u32 vfp_double_fcvts(ARMul_State* state, int sd, int unused, int dm, u32 fpscr) {
    struct vfp_double vdm;
    struct vfp_single vsd;
    int tm;
    u32 exceptions = 0;

    LOG_TRACE(Core_ARM11, "In {}", __FUNCTION__);
    exceptions |= vfp_double_unpack(&vdm, vfp_get_double(state, dm), fpscr);

    tm = vfp_double_type(&vdm);

    /*
     * If we have a signalling NaN, signal invalid operation.
     */
    if (tm == VFP_SNAN)
        exceptions = FPSCR_IOC;

    if (tm & VFP_DENORMAL)
        vfp_double_normalise_denormal(&vdm);

    vsd.sign = vdm.sign;
    vsd.significand = vfp_hi64to32jamming(vdm.significand);

    /*
     * If we have an infinity or a NaN, the exponent must be 255
     */
    if (tm & (VFP_INFINITY | VFP_NAN)) {
        vsd.exponent = 255;
        if (tm == VFP_QNAN)
            vsd.significand |= VFP_SINGLE_SIGNIFICAND_QNAN;
        goto pack_nan;
    } else if (tm & VFP_ZERO)
        vsd.exponent = 0;
    else
        vsd.exponent = vdm.exponent - (1023 - 127);

    return vfp_single_normaliseround(state, sd, &vsd, fpscr, exceptions, "fcvts");

pack_nan:
    vfp_put_float(state, vfp_single_pack(&vsd), sd);
    return exceptions;
}

static u32 vfp_double_fuito(ARMul_State* state, int dd, int unused, int dm, u32 fpscr) {
    struct vfp_double vdm;
    u32 m = vfp_get_float(state, dm);

    LOG_TRACE(Core_ARM11, "In {}", __FUNCTION__);
    vdm.sign = 0;
    vdm.exponent = 1023 + 63 - 1;
    vdm.significand = (u64)m;

    return vfp_double_normaliseround(state, dd, &vdm, fpscr, 0, "fuito");
}

static u32 vfp_double_fsito(ARMul_State* state, int dd, int unused, int dm, u32 fpscr) {
    struct vfp_double vdm;
    u32 m = vfp_get_float(state, dm);

    LOG_TRACE(Core_ARM11, "In {}", __FUNCTION__);
    vdm.sign = (m & 0x80000000) >> 16;
    vdm.exponent = 1023 + 63 - 1;
    vdm.significand = vdm.sign ? (~m + 1) : m;

    return vfp_double_normaliseround(state, dd, &vdm, fpscr, 0, "fsito");
}

static u32 vfp_double_ftoui(ARMul_State* state, int sd, int unused, int dm, u32 fpscr) {
    struct vfp_double vdm;
    u32 d, exceptions = 0;
    int rmode = fpscr & FPSCR_RMODE_MASK;
    int tm;

    LOG_TRACE(Core_ARM11, "In {}", __FUNCTION__);
    exceptions |= vfp_double_unpack(&vdm, vfp_get_double(state, dm), fpscr);

    /*
     * Do we have a denormalised number?
     */
    tm = vfp_double_type(&vdm);
    if (tm & VFP_DENORMAL)
        exceptions |= FPSCR_IDC;

    if (tm & VFP_NAN)
        vdm.sign = 1;

    if (vdm.exponent >= 1023 + 32) {
        d = vdm.sign ? 0 : 0xffffffff;
        exceptions = FPSCR_IOC;
    } else if (vdm.exponent >= 1023) {
        int shift = 1023 + 63 - vdm.exponent;
        u64 rem, incr = 0;

        /*
         * 2^0 <= m < 2^32-2^8
         */
        d = (u32)((vdm.significand << 1) >> shift);
        rem = vdm.significand << (65 - shift);

        if (rmode == FPSCR_ROUND_NEAREST) {
            incr = 0x8000000000000000ULL;
            if ((d & 1) == 0)
                incr -= 1;
        } else if (rmode == FPSCR_ROUND_TOZERO) {
            incr = 0;
        } else if ((rmode == FPSCR_ROUND_PLUSINF) ^ (vdm.sign != 0)) {
            incr = ~0ULL;
        }

        if ((rem + incr) < rem) {
            if (d < 0xffffffff)
                d += 1;
            else
                exceptions |= FPSCR_IOC;
        }

        if (d && vdm.sign) {
            d = 0;
            exceptions |= FPSCR_IOC;
        } else if (rem)
            exceptions |= FPSCR_IXC;
    } else {
        d = 0;
        if (vdm.exponent | vdm.significand) {
            if (rmode == FPSCR_ROUND_NEAREST) {
                if (vdm.exponent >= 1022) {
                    d = vdm.sign ? 0 : 1;
                    exceptions |= vdm.sign ? FPSCR_IOC : FPSCR_IXC;
                } else {
                    exceptions |= FPSCR_IXC;
                }
            } else if (rmode == FPSCR_ROUND_PLUSINF && vdm.sign == 0) {
                d = 1;
                exceptions |= FPSCR_IXC;
            } else if (rmode == FPSCR_ROUND_MINUSINF) {
                exceptions |= vdm.sign ? FPSCR_IOC : FPSCR_IXC;
            } else {
                exceptions |= FPSCR_IXC;
            }
        }
    }

    LOG_TRACE(Core_ARM11, "VFP: ftoui: d(s{})={:08x} exceptions={:08x}", sd, d, exceptions);

    vfp_put_float(state, d, sd);

    return exceptions;
}

static u32 vfp_double_ftouiz(ARMul_State* state, int sd, int unused, int dm, u32 fpscr) {
    LOG_TRACE(Core_ARM11, "In {}", __FUNCTION__);
    return vfp_double_ftoui(state, sd, unused, dm,
                            (fpscr & ~FPSCR_RMODE_MASK) | FPSCR_ROUND_TOZERO);
}

static u32 vfp_double_ftosi(ARMul_State* state, int sd, int unused, int dm, u32 fpscr) {
    struct vfp_double vdm;
    u32 d, exceptions = 0;
    int rmode = fpscr & FPSCR_RMODE_MASK;
    int tm;

    LOG_TRACE(Core_ARM11, "In {}", __FUNCTION__);
    exceptions |= vfp_double_unpack(&vdm, vfp_get_double(state, dm), fpscr);
    vfp_double_dump("VDM", &vdm);

    /*
     * Do we have denormalised number?
     */
    tm = vfp_double_type(&vdm);
    if (tm & VFP_DENORMAL)
        exceptions |= FPSCR_IDC;

    if (tm & VFP_NAN) {
        d = 0;
        exceptions |= FPSCR_IOC;
    } else if (vdm.exponent >= 1023 + 31) {
        d = 0x7fffffff;
        if (vdm.sign)
            d = ~d;
        exceptions |= FPSCR_IOC;
    } else if (vdm.exponent >= 1023) {
        int shift = 1023 + 63 - vdm.exponent; /* 58 */
        u64 rem, incr = 0;

        d = (u32)((vdm.significand << 1) >> shift);
        rem = vdm.significand << (65 - shift);

        if (rmode == FPSCR_ROUND_NEAREST) {
            incr = 0x8000000000000000ULL;
            if ((d & 1) == 0)
                incr -= 1;
        } else if (rmode == FPSCR_ROUND_TOZERO) {
            incr = 0;
        } else if ((rmode == FPSCR_ROUND_PLUSINF) ^ (vdm.sign != 0)) {
            incr = ~0ULL;
        }

        if ((rem + incr) < rem && d < 0xffffffff)
            d += 1;
        if (d > (0x7fffffffU + (vdm.sign != 0))) {
            d = (0x7fffffffU + (vdm.sign != 0));
            exceptions |= FPSCR_IOC;
        } else if (rem)
            exceptions |= FPSCR_IXC;

        if (vdm.sign)
            d = (~d + 1);
    } else {
        d = 0;
        if (vdm.exponent | vdm.significand) {
            exceptions |= FPSCR_IXC;
            if (rmode == FPSCR_ROUND_NEAREST) {
                if (vdm.exponent >= 1022) {
                    d = vdm.sign ? 0xffffffff : 1;
                } else {
                    d = 0;
                }
            } else if (rmode == FPSCR_ROUND_PLUSINF && vdm.sign == 0) {
                d = 1;
            } else if (rmode == FPSCR_ROUND_MINUSINF && vdm.sign) {
                d = 0xffffffff;
            }
        }
    }

    LOG_TRACE(Core_ARM11, "VFP: ftosi: d(s{})={:08x} exceptions={:08x}", sd, d, exceptions);

    vfp_put_float(state, (s32)d, sd);

    return exceptions;
}

static u32 vfp_double_ftosiz(ARMul_State* state, int dd, int unused, int dm, u32 fpscr) {
    LOG_TRACE(Core_ARM11, "In {}", __FUNCTION__);
    return vfp_double_ftosi(state, dd, unused, dm,
                            (fpscr & ~FPSCR_RMODE_MASK) | FPSCR_ROUND_TOZERO);
}

static struct op fops_ext[] = {
    {vfp_double_fcpy, 0},  // 0x00000000 - FEXT_FCPY
    {vfp_double_fabs, 0},  // 0x00000001 - FEXT_FABS
    {vfp_double_fneg, 0},  // 0x00000002 - FEXT_FNEG
    {vfp_double_fsqrt, 0}, // 0x00000003 - FEXT_FSQRT
    {nullptr, 0},
    {nullptr, 0},
    {nullptr, 0},
    {nullptr, 0},
    {vfp_double_fcmp, OP_SCALAR},   // 0x00000008 - FEXT_FCMP
    {vfp_double_fcmpe, OP_SCALAR},  // 0x00000009 - FEXT_FCMPE
    {vfp_double_fcmpz, OP_SCALAR},  // 0x0000000A - FEXT_FCMPZ
    {vfp_double_fcmpez, OP_SCALAR}, // 0x0000000B - FEXT_FCMPEZ
    {nullptr, 0},
    {nullptr, 0},
    {nullptr, 0},
    {vfp_double_fcvts, OP_SCALAR | OP_DD}, // 0x0000000F - FEXT_FCVT
    {vfp_double_fuito, OP_SCALAR | OP_SM}, // 0x00000010 - FEXT_FUITO
    {vfp_double_fsito, OP_SCALAR | OP_SM}, // 0x00000011 - FEXT_FSITO
    {nullptr, 0},
    {nullptr, 0},
    {nullptr, 0},
    {nullptr, 0},
    {nullptr, 0},
    {nullptr, 0},
    {vfp_double_ftoui, OP_SCALAR | OP_SD},  // 0x00000018 - FEXT_FTOUI
    {vfp_double_ftouiz, OP_SCALAR | OP_SD}, // 0x00000019 - FEXT_FTOUIZ
    {vfp_double_ftosi, OP_SCALAR | OP_SD},  // 0x0000001A - FEXT_FTOSI
    {vfp_double_ftosiz, OP_SCALAR | OP_SD}, // 0x0000001B - FEXT_FTOSIZ
};

static u32 vfp_double_fadd_nonnumber(struct vfp_double* vdd, struct vfp_double* vdn,
                                     struct vfp_double* vdm, u32 fpscr) {
    struct vfp_double* vdp;
    u32 exceptions = 0;
    int tn, tm;

    tn = vfp_double_type(vdn);
    tm = vfp_double_type(vdm);

    if (tn & tm & VFP_INFINITY) {
        /*
         * Two infinities.  Are they different signs?
         */
        if (vdn->sign ^ vdm->sign) {
            /*
             * different signs -> invalid
             */
            exceptions = FPSCR_IOC;
            vdp = &vfp_double_default_qnan;
        } else {
            /*
             * same signs -> valid
             */
            vdp = vdn;
        }
    } else if (tn & VFP_INFINITY && tm & VFP_NUMBER) {
        /*
         * One infinity and one number -> infinity
         */
        vdp = vdn;
    } else {
        /*
         * 'n' is a NaN of some type
         */
        return vfp_propagate_nan(vdd, vdn, vdm, fpscr);
    }
    *vdd = *vdp;
    return exceptions;
}

u32 vfp_double_add(struct vfp_double* vdd, struct vfp_double* vdn, struct vfp_double* vdm,
                   u32 fpscr) {
    u32 exp_diff;
    u64 m_sig;

    if (vdn->significand & (1ULL << 63) || vdm->significand & (1ULL << 63)) {
        LOG_INFO(Core_ARM11, "VFP: bad FP values in {}", __func__);
        vfp_double_dump("VDN", vdn);
        vfp_double_dump("VDM", vdm);
    }

    /*
     * Ensure that 'n' is the largest magnitude number.  Note that
     * if 'n' and 'm' have equal exponents, we do not swap them.
     * This ensures that NaN propagation works correctly.
     */
    if (vdn->exponent < vdm->exponent) {
        std::swap(vdm, vdn);
    }

    /*
     * Is 'n' an infinity or a NaN?  Note that 'm' may be a number,
     * infinity or a NaN here.
     */
    if (vdn->exponent == 2047)
        return vfp_double_fadd_nonnumber(vdd, vdn, vdm, fpscr);

    /*
     * We have two proper numbers, where 'vdn' is the larger magnitude.
     *
     * Copy 'n' to 'd' before doing the arithmetic.
     */
    *vdd = *vdn;

    /*
     * Align 'm' with the result.
     */
    exp_diff = vdn->exponent - vdm->exponent;
    m_sig = vfp_shiftright64jamming(vdm->significand, exp_diff);

    /*
     * If the signs are different, we are really subtracting.
     */
    if (vdn->sign ^ vdm->sign) {
        m_sig = vdn->significand - m_sig;
        if ((s64)m_sig < 0) {
            vdd->sign = vfp_sign_negate(vdd->sign);
            m_sig = (~m_sig + 1);
        } else if (m_sig == 0) {
            vdd->sign = (fpscr & FPSCR_RMODE_MASK) == FPSCR_ROUND_MINUSINF ? 0x8000 : 0;
        }
    } else {
        m_sig += vdn->significand;
    }
    vdd->significand = m_sig;

    return 0;
}

u32 vfp_double_multiply(struct vfp_double* vdd, struct vfp_double* vdn, struct vfp_double* vdm,
                        u32 fpscr) {
    vfp_double_dump("VDN", vdn);
    vfp_double_dump("VDM", vdm);

    /*
     * Ensure that 'n' is the largest magnitude number.  Note that
     * if 'n' and 'm' have equal exponents, we do not swap them.
     * This ensures that NaN propagation works correctly.
     */
    if (vdn->exponent < vdm->exponent) {
        std::swap(vdm, vdn);
        LOG_TRACE(Core_ARM11, "VFP: swapping M <-> N");
    }

    vdd->sign = vdn->sign ^ vdm->sign;

    /*
     * If 'n' is an infinity or NaN, handle it.  'm' may be anything.
     */
    if (vdn->exponent == 2047) {
        if (vdn->significand || (vdm->exponent == 2047 && vdm->significand))
            return vfp_propagate_nan(vdd, vdn, vdm, fpscr);
        if ((vdm->exponent | vdm->significand) == 0) {
            *vdd = vfp_double_default_qnan;
            return FPSCR_IOC;
        }
        vdd->exponent = vdn->exponent;
        vdd->significand = 0;
        return 0;
    }

    /*
     * If 'm' is zero, the result is always zero.  In this case,
     * 'n' may be zero or a number, but it doesn't matter which.
     */
    if ((vdm->exponent | vdm->significand) == 0) {
        vdd->exponent = 0;
        vdd->significand = 0;
        return 0;
    }

    /*
     * We add 2 to the destination exponent for the same reason
     * as the addition case - though this time we have +1 from
     * each input operand.
     */
    vdd->exponent = vdn->exponent + vdm->exponent - 1023 + 2;
    vdd->significand = vfp_hi64multiply64(vdn->significand, vdm->significand);

    vfp_double_dump("VDD", vdd);
    return 0;
}

#define NEG_MULTIPLY (1 << 0)
#define NEG_SUBTRACT (1 << 1)

static u32 vfp_double_multiply_accumulate(ARMul_State* state, int dd, int dn, int dm, u32 fpscr,
                                          u32 negate, const char* func) {
    struct vfp_double vdd, vdp, vdn, vdm;
    u32 exceptions = 0;

    exceptions |= vfp_double_unpack(&vdn, vfp_get_double(state, dn), fpscr);
    if (vdn.exponent == 0 && vdn.significand)
        vfp_double_normalise_denormal(&vdn);

    exceptions |= vfp_double_unpack(&vdm, vfp_get_double(state, dm), fpscr);
    if (vdm.exponent == 0 && vdm.significand)
        vfp_double_normalise_denormal(&vdm);

    exceptions |= vfp_double_multiply(&vdp, &vdn, &vdm, fpscr);
    if (negate & NEG_MULTIPLY)
        vdp.sign = vfp_sign_negate(vdp.sign);

    exceptions |= vfp_double_unpack(&vdn, vfp_get_double(state, dd), fpscr);
    if (vdn.exponent == 0 && vdn.significand != 0)
        vfp_double_normalise_denormal(&vdn);

    if (negate & NEG_SUBTRACT)
        vdn.sign = vfp_sign_negate(vdn.sign);

    exceptions |= vfp_double_add(&vdd, &vdn, &vdp, fpscr);

    return vfp_double_normaliseround(state, dd, &vdd, fpscr, exceptions, func);
}

/*
 * Standard operations
 */

/*
 * sd = sd + (sn * sm)
 */
static u32 vfp_double_fmac(ARMul_State* state, int dd, int dn, int dm, u32 fpscr) {
    LOG_TRACE(Core_ARM11, "In {}", __FUNCTION__);
    return vfp_double_multiply_accumulate(state, dd, dn, dm, fpscr, 0, "fmac");
}

/*
 * sd = sd - (sn * sm)
 */
static u32 vfp_double_fnmac(ARMul_State* state, int dd, int dn, int dm, u32 fpscr) {
    LOG_TRACE(Core_ARM11, "In {}", __FUNCTION__);
    return vfp_double_multiply_accumulate(state, dd, dn, dm, fpscr, NEG_MULTIPLY, "fnmac");
}

/*
 * sd = -sd + (sn * sm)
 */
static u32 vfp_double_fmsc(ARMul_State* state, int dd, int dn, int dm, u32 fpscr) {
    LOG_TRACE(Core_ARM11, "In {}", __FUNCTION__);
    return vfp_double_multiply_accumulate(state, dd, dn, dm, fpscr, NEG_SUBTRACT, "fmsc");
}

/*
 * sd = -sd - (sn * sm)
 */
static u32 vfp_double_fnmsc(ARMul_State* state, int dd, int dn, int dm, u32 fpscr) {
    LOG_TRACE(Core_ARM11, "In {}", __FUNCTION__);
    return vfp_double_multiply_accumulate(state, dd, dn, dm, fpscr, NEG_SUBTRACT | NEG_MULTIPLY,
                                          "fnmsc");
}

/*
 * sd = sn * sm
 */
static u32 vfp_double_fmul(ARMul_State* state, int dd, int dn, int dm, u32 fpscr) {
    struct vfp_double vdd, vdn, vdm;
    u32 exceptions = 0;

    LOG_TRACE(Core_ARM11, "In {}", __FUNCTION__);
    exceptions |= vfp_double_unpack(&vdn, vfp_get_double(state, dn), fpscr);
    if (vdn.exponent == 0 && vdn.significand)
        vfp_double_normalise_denormal(&vdn);

    exceptions |= vfp_double_unpack(&vdm, vfp_get_double(state, dm), fpscr);
    if (vdm.exponent == 0 && vdm.significand)
        vfp_double_normalise_denormal(&vdm);

    exceptions |= vfp_double_multiply(&vdd, &vdn, &vdm, fpscr);
    return vfp_double_normaliseround(state, dd, &vdd, fpscr, exceptions, "fmul");
}

/*
 * sd = -(sn * sm)
 */
static u32 vfp_double_fnmul(ARMul_State* state, int dd, int dn, int dm, u32 fpscr) {
    struct vfp_double vdd, vdn, vdm;
    u32 exceptions = 0;

    LOG_TRACE(Core_ARM11, "In {}", __FUNCTION__);
    exceptions |= vfp_double_unpack(&vdn, vfp_get_double(state, dn), fpscr);
    if (vdn.exponent == 0 && vdn.significand)
        vfp_double_normalise_denormal(&vdn);

    exceptions |= vfp_double_unpack(&vdm, vfp_get_double(state, dm), fpscr);
    if (vdm.exponent == 0 && vdm.significand)
        vfp_double_normalise_denormal(&vdm);

    exceptions |= vfp_double_multiply(&vdd, &vdn, &vdm, fpscr);
    vdd.sign = vfp_sign_negate(vdd.sign);

    return vfp_double_normaliseround(state, dd, &vdd, fpscr, exceptions, "fnmul");
}

/*
 * sd = sn + sm
 */
static u32 vfp_double_fadd(ARMul_State* state, int dd, int dn, int dm, u32 fpscr) {
    struct vfp_double vdd, vdn, vdm;
    u32 exceptions = 0;

    LOG_TRACE(Core_ARM11, "In {}", __FUNCTION__);
    exceptions |= vfp_double_unpack(&vdn, vfp_get_double(state, dn), fpscr);
    if (vdn.exponent == 0 && vdn.significand)
        vfp_double_normalise_denormal(&vdn);

    exceptions |= vfp_double_unpack(&vdm, vfp_get_double(state, dm), fpscr);
    if (vdm.exponent == 0 && vdm.significand)
        vfp_double_normalise_denormal(&vdm);

    exceptions |= vfp_double_add(&vdd, &vdn, &vdm, fpscr);

    return vfp_double_normaliseround(state, dd, &vdd, fpscr, exceptions, "fadd");
}

/*
 * sd = sn - sm
 */
static u32 vfp_double_fsub(ARMul_State* state, int dd, int dn, int dm, u32 fpscr) {
    struct vfp_double vdd, vdn, vdm;
    u32 exceptions = 0;

    LOG_TRACE(Core_ARM11, "In {}", __FUNCTION__);
    exceptions |= vfp_double_unpack(&vdn, vfp_get_double(state, dn), fpscr);
    if (vdn.exponent == 0 && vdn.significand)
        vfp_double_normalise_denormal(&vdn);

    exceptions |= vfp_double_unpack(&vdm, vfp_get_double(state, dm), fpscr);
    if (vdm.exponent == 0 && vdm.significand)
        vfp_double_normalise_denormal(&vdm);

    /*
     * Subtraction is like addition, but with a negated operand.
     */
    vdm.sign = vfp_sign_negate(vdm.sign);

    exceptions |= vfp_double_add(&vdd, &vdn, &vdm, fpscr);

    return vfp_double_normaliseround(state, dd, &vdd, fpscr, exceptions, "fsub");
}

/*
 * sd = sn / sm
 */
static u32 vfp_double_fdiv(ARMul_State* state, int dd, int dn, int dm, u32 fpscr) {
    struct vfp_double vdd, vdn, vdm;
    u32 exceptions = 0;
    int tm, tn;

    LOG_TRACE(Core_ARM11, "In {}", __FUNCTION__);
    exceptions |= vfp_double_unpack(&vdn, vfp_get_double(state, dn), fpscr);
    exceptions |= vfp_double_unpack(&vdm, vfp_get_double(state, dm), fpscr);

    vdd.sign = vdn.sign ^ vdm.sign;

    tn = vfp_double_type(&vdn);
    tm = vfp_double_type(&vdm);

    /*
     * Is n a NAN?
     */
    if (tn & VFP_NAN)
        goto vdn_nan;

    /*
     * Is m a NAN?
     */
    if (tm & VFP_NAN)
        goto vdm_nan;

    /*
     * If n and m are infinity, the result is invalid
     * If n and m are zero, the result is invalid
     */
    if (tm & tn & (VFP_INFINITY | VFP_ZERO))
        goto invalid;

    /*
     * If n is infinity, the result is infinity
     */
    if (tn & VFP_INFINITY)
        goto infinity;

    /*
     * If m is zero, raise div0 exceptions
     */
    if (tm & VFP_ZERO)
        goto divzero;

    /*
     * If m is infinity, or n is zero, the result is zero
     */
    if (tm & VFP_INFINITY || tn & VFP_ZERO)
        goto zero;

    if (tn & VFP_DENORMAL)
        vfp_double_normalise_denormal(&vdn);
    if (tm & VFP_DENORMAL)
        vfp_double_normalise_denormal(&vdm);

    /*
     * Ok, we have two numbers, we can perform division.
     */
    vdd.exponent = vdn.exponent - vdm.exponent + 1023 - 1;
    vdm.significand <<= 1;
    if (vdm.significand <= (2 * vdn.significand)) {
        vdn.significand >>= 1;
        vdd.exponent++;
    }
    vdd.significand = vfp_estimate_div128to64(vdn.significand, 0, vdm.significand);
    if ((vdd.significand & 0x1ff) <= 2) {
        u64 termh, terml, remh, reml;
        mul64to128(&termh, &terml, vdm.significand, vdd.significand);
        sub128(&remh, &reml, vdn.significand, 0, termh, terml);
        while ((s64)remh < 0) {
            vdd.significand -= 1;
            add128(&remh, &reml, remh, reml, 0, vdm.significand);
        }
        vdd.significand |= (reml != 0);
    }
    return vfp_double_normaliseround(state, dd, &vdd, fpscr, 0, "fdiv");

vdn_nan:
    exceptions |= vfp_propagate_nan(&vdd, &vdn, &vdm, fpscr);
pack:
    vfp_put_double(state, vfp_double_pack(&vdd), dd);
    return exceptions;

vdm_nan:
    exceptions |= vfp_propagate_nan(&vdd, &vdm, &vdn, fpscr);
    goto pack;

zero:
    vdd.exponent = 0;
    vdd.significand = 0;
    goto pack;

divzero:
    exceptions |= FPSCR_DZC;
infinity:
    vdd.exponent = 2047;
    vdd.significand = 0;
    goto pack;

invalid:
    vfp_put_double(state, vfp_double_pack(&vfp_double_default_qnan), dd);
    return FPSCR_IOC;
}

static struct op fops[] = {
    {vfp_double_fmac, 0},  {vfp_double_fmsc, 0},  {vfp_double_fmul, 0},
    {vfp_double_fadd, 0},  {vfp_double_fnmac, 0}, {vfp_double_fnmsc, 0},
    {vfp_double_fnmul, 0}, {vfp_double_fsub, 0},  {vfp_double_fdiv, 0},
};

#define FREG_BANK(x) ((x) & 0x0c)
#define FREG_IDX(x) ((x) & 3)

u32 vfp_double_cpdo(ARMul_State* state, u32 inst, u32 fpscr) {
    u32 op = inst & FOP_MASK;
    u32 exceptions = 0;
    unsigned int dest;
    unsigned int dn = vfp_get_dn(inst);
    unsigned int dm;
    unsigned int vecitr, veclen, vecstride;
    struct op* fop;

    LOG_TRACE(Core_ARM11, "In {}", __FUNCTION__);
    vecstride = (1 + ((fpscr & FPSCR_STRIDE_MASK) == FPSCR_STRIDE_MASK));

    fop = (op == FOP_EXT) ? &fops_ext[FEXT_TO_IDX(inst)] : &fops[FOP_TO_IDX(op)];

    /*
     * fcvtds takes an sN register number as destination, not dN.
     * It also always operates on scalars.
     */
    if (fop->flags & OP_SD)
        dest = vfp_get_sd(inst);
    else
        dest = vfp_get_dd(inst);

    /*
     * f[us]ito takes a sN operand, not a dN operand.
     */
    if (fop->flags & OP_SM)
        dm = vfp_get_sm(inst);
    else
        dm = vfp_get_dm(inst);

    /*
     * If destination bank is zero, vector length is always '1'.
     * ARM DDI0100F C5.1.3, C5.3.2.
     */
    if ((fop->flags & OP_SCALAR) || (FREG_BANK(dest) == 0))
        veclen = 0;
    else
        veclen = fpscr & FPSCR_LENGTH_MASK;

    LOG_TRACE(Core_ARM11, "VFP: vecstride={} veclen={}", vecstride,
              (veclen >> FPSCR_LENGTH_BIT) + 1);

    if (!fop->fn) {
        LOG_TRACE(Core_ARM11, "VFP: could not find double op {}", FEXT_TO_IDX(inst));
        goto invalid;
    }

    for (vecitr = 0; vecitr <= veclen; vecitr += 1 << FPSCR_LENGTH_BIT) {
        u32 except;
        [[maybe_unused]] char type;

        type = (fop->flags & OP_SD) ? 's' : 'd';
        if (op == FOP_EXT)
            LOG_TRACE(Core_ARM11, "VFP: itr{} ({}{}) = op[{}] (d{})", vecitr >> FPSCR_LENGTH_BIT,
                      type, dest, dn, dm);
        else
            LOG_TRACE(Core_ARM11, "VFP: itr{} ({}{}) = (d{}) op[{}] (d{})",
                      vecitr >> FPSCR_LENGTH_BIT, type, dest, dn, FOP_TO_IDX(op), dm);

        except = fop->fn(state, dest, dn, dm, fpscr);
        LOG_TRACE(Core_ARM11, "VFP: itr{}: exceptions={:08x}", vecitr >> FPSCR_LENGTH_BIT, except);

        exceptions |= except & ~VFP_NAN_FLAG;

        /*
         * CHECK: It appears to be undefined whether we stop when
         * we encounter an exception.  We continue.
         */
        dest = FREG_BANK(dest) + ((FREG_IDX(dest) + vecstride) & 3);
        dn = FREG_BANK(dn) + ((FREG_IDX(dn) + vecstride) & 3);
        if (FREG_BANK(dm) != 0)
            dm = FREG_BANK(dm) + ((FREG_IDX(dm) + vecstride) & 3);
    }
    return exceptions;

invalid:
    return ~0;
}
