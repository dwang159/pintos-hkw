#ifndef FIXED-POINT_H
#define FIXED-POINT_H

typedef fixed_point_t int;
#define FRAC_BITS 14
#define INT_BITS (31-FRAC_BITS)
#define FP_SCALE (1 << FRAC_BITS)

#define int_to_fp(n)   (n * FP_SCALE)
#define fp_to_int(x)   ( 
#define fpadd(x, y)    (x + y)
#define fpaddint(x, n) (x + int_to_fp(n))
#define fpsub(x, y)    (x - y)
#define fpsubint(x, n) (x - int_to_fp(n))
#define fpmul(x, y)    (((int64_t )x) * y / FP_SCALE)
#define fpmulint(x, n) (x * n)
#define fpdiv(x, y)    (((int 64_t )x) * FP_SCALE / y)
#define fpdivint(x, n) (x / n)

#endif /* FIXED-POINT_H */



