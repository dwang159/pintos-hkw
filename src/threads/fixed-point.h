#ifndef FIXED_POINT_H
#define FIXED_POINT_H
#include <stdint.h>

typedef int fixed_point_t;
#define FRAC_BITS 14
#define INT_BITS (31-FRAC_BITS)
#define FP_SCALE (1 << FRAC_BITS)

#define inttofp(n)   (n * FP_SCALE)
#define fptoint(x)   (x <= 0 ? x + (FP_SCALE / 2)/ FP_SCALE : \
                                    x - (FP_SCALE / 2)/ FP_SCALE)
#define fpadd(x, y)    (x + y)
#define fpaddint(x, n) (x + inttofp(n))
#define fpsub(x, y)    (x - y)
#define fpsubint(x, n) (x - inttofp(n))
#define fpmul(x, y)    ((fixed_point_t) ((int64_t )x * y / FP_SCALE))
#define fpmulint(x, n) (x * n)
#define fpdiv(x, y)    ((fixed_point_t) ((int64_t )x * FP_SCALE / y))
#define fpdivint(x, n) (x / n)


#endif /* FIXED_POINT_H */
