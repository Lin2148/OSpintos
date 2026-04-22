/** fixed-point arithmetic operations implemented in C
 *  in this file n is int, x and y are FP, format where p(17) + q(14) = 31, F is 1 << q 
*/
#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

#include <stdint.h> 

#define F (1 << 14)

/** incline let this func. copy-paste to where call it, not traditional func call */
static inline int int_to_fp(int n) {
    return n * F;
}

static inline int fp_to_int_round_zero(int x) {
    return x / F;
}

static inline int fp_to_int_round_nearest(int x) {
    if (x >= 0) {
        return (x + F / 2) / F;
    } else {
        return (x - F / 2) / F;
    }
}

static inline int fp_add(int x, int y) {
    return x + y;
}

static inline int fp_sub(int x, int y) {
    return x - y;
}

static inline int fp_int_add(int x, int n) {
    return x + n * F;
}

static inline int fp_int_sub(int x, int n) {
    return x - n * F;
}

static inline int fp_mul(int x, int y) {
    return ((int64_t) x) * y / F;
}

static inline int fp_int_mul(int x, int n) {
    return x * n;
}

static inline int fp_div(int x, int y) {
    return ((int64_t) x) * F / y;
}

static inline int fp_int_div(int x, int n) {
    return x / n;
}

#endif /* threads/fixed_point.h */