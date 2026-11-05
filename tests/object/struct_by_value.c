/* The C half of struct_by_value.occ. Compiled by cc, so the convention
   under test is whatever cc implements, not whatever we think it is. */
#include <stdint.h>

struct TwoInt    { int64_t a; int64_t b; };          /* INTEGER, INTEGER */
struct TwoDbl    { double x; double y; };            /* SSE, SSE */
struct Mixed     { int64_t n; double d; };           /* INTEGER, SSE */
struct Packed    { int8_t a; int16_t b; int32_t c; };/* one INTEGER eightbyte */
struct FloatPair { float u; float v; };              /* one SSE eightbyte */
struct Big       { int64_t a, b, c, d; };            /* MEMORY */

int c_two_int(struct TwoInt s)       { return s.a == 11 && s.b == 22; }
int c_two_dbl(struct TwoDbl s)       { return s.x == 1.5 && s.y == 2.5; }
int c_mixed(struct Mixed s)          { return s.n == 7 && s.d == 3.25; }
int c_packed(struct Packed s)        { return s.a == 1 && s.b == 2 && s.c == 3; }
int c_float_pair(struct FloatPair s) { return s.u == 0.5f && s.v == 0.25f; }
int c_big(struct Big s)              { return s.a == 1 && s.b == 2 && s.c == 3 && s.d == 4; }

/* a struct among other arguments, to check the register accounting */
int c_around(int64_t lead, struct Mixed s, int64_t tail, double fp) {
    return lead == 100 && s.n == 7 && s.d == 3.25 && tail == 200 && fp == 9.5;
}
int c_two_structs(struct TwoInt a, struct TwoDbl b) {
    return a.a == 11 && a.b == 22 && b.x == 1.5 && b.y == 2.5;
}

struct TwoInt c_ret_two_int(void) { struct TwoInt s = {11, 22}; return s; }
struct TwoDbl c_ret_two_dbl(void) { struct TwoDbl s = {1.5, 2.5}; return s; }
struct Mixed  c_ret_mixed(void)   { struct Mixed  s = {7, 3.25}; return s; }
struct Big    c_ret_big(int64_t s){ struct Big b = {s, s+1, s+2, s+3}; return b; }

/* and the other direction: C calling Occult */
long occ_sum_two_int(struct TwoInt s);
long occ_sum_big(struct Big s);
struct TwoInt occ_make_two_int(int64_t a, int64_t b);
struct Big    occ_make_big(int64_t seed);

int c_calls_occult(void) {
    struct TwoInt ti = {11, 22};
    struct Big bg = {1, 2, 3, 4};
    if (occ_sum_two_int(ti) != 33) return 0;
    if (occ_sum_big(bg) != 10) return 0;
    struct TwoInt r = occ_make_two_int(5, 9);
    if (r.a != 5 || r.b != 9) return 0;
    struct Big b = occ_make_big(10);
    if (b.a != 10 || b.b != 11 || b.c != 12 || b.d != 13) return 0;
    return 1;
}
