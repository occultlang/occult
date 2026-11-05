/* The same structs, declared in C. If Occult lays them out the way C
   does, every offset and size below matches and the two languages can
   share a pointer. Linked against c_struct_layout.occ, which holds main. */
#include <stddef.h>
#include <stdint.h>

struct Small { int8_t a; int32_t b; };
struct Mixed { int8_t a; int64_t b; int16_t c; int8_t d; };
struct Floats { float f; double d; int32_t i; };
struct Nested { int8_t tag; struct Small inner; int8_t trailer; };

/* Occult asks for these rather than hard-coding them, so a failure says
   which number disagreed instead of just failing. */
long c_sizeof_small(void)   { return (long)sizeof(struct Small); }
long c_sizeof_mixed(void)   { return (long)sizeof(struct Mixed); }
long c_sizeof_floats(void)  { return (long)sizeof(struct Floats); }
long c_sizeof_nested(void)  { return (long)sizeof(struct Nested); }

long c_off_small_b(void)    { return (long)offsetof(struct Small, b); }
long c_off_mixed_b(void)    { return (long)offsetof(struct Mixed, b); }
long c_off_mixed_c(void)    { return (long)offsetof(struct Mixed, c); }
long c_off_mixed_d(void)    { return (long)offsetof(struct Mixed, d); }
long c_off_floats_d(void)   { return (long)offsetof(struct Floats, d); }
long c_off_floats_i(void)   { return (long)offsetof(struct Floats, i); }
long c_off_nested_inner(void)   { return (long)offsetof(struct Nested, inner); }
long c_off_nested_trailer(void) { return (long)offsetof(struct Nested, trailer); }

/* C fills a struct; Occult reads it back member by member. This is the
   part a size table cannot fake: the bytes have to be where C put them. */
void c_fill_mixed(struct Mixed *m) {
    m->a = -7;
    m->b = 1234567890123;
    m->c = -300;
    m->d = 42;
}

void c_fill_floats(struct Floats *f) { f->f = 1.5f; f->d = 2.25; f->i = -9; }
int  c_check_floats(const struct Floats *f) { return f->f == 0.5f && f->d == -4.75 && f->i == 7; }

void c_fill_nested(struct Nested *n) { n->tag = 3; n->inner.a = -2; n->inner.b = 70000; n->trailer = 9; }
int  c_check_nested(const struct Nested *n) {
    return n->tag == 5 && n->inner.a == 6 && n->inner.b == -70000 && n->trailer == 8;
}

/* And the other direction: Occult fills it, C checks it. */
int c_check_mixed(const struct Mixed *m) {
    return m->a == 11 && m->b == -99887766554433L && m->c == 1000 && m->d == -1;
}
