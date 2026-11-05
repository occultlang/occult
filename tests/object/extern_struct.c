/* The C side of extern_struct.occ: one struct it transcribes and one it is
   only ever handed a pointer to. */
#include <stdint.h>
#include <stdlib.h>

struct c_point { int32_t x; int32_t y; };

struct c_handle { int64_t guard; int32_t value; char pad[29]; };

int32_t c_sum(const struct c_point* p) { return p->x + p->y; }

struct c_handle* c_make(void) {
    struct c_handle* h = malloc(sizeof *h);
    h->guard = 0x5a5a5a5a;
    h->value = 1234;
    return h;
}

int32_t c_value(const struct c_handle* h) {
    return h->guard == 0x5a5a5a5a ? h->value : -1;
}

int64_t c_free(struct c_handle* h) { free(h); return 0; }
