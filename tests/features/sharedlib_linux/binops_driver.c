#include <dlfcn.h>
#include <stdio.h>

typedef long (*binop)(long, long);
typedef long (*unop)(long);

int main(int argc, char** argv) {
    const char* so = argc > 1 ? argv[1] : "./libbinops.so";
    void* h = dlopen(so, RTLD_NOW);
    if (!h) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }

    binop add = (binop) dlsym(h, "add");
    binop mul = (binop) dlsym(h, "mul");
    unop  neg = (unop)  dlsym(h, "neg");
    unop  square = (unop) dlsym(h, "square");
    if (!add || !mul || !neg || !square) {
        fprintf(stderr, "missing symbol\n");
        return 2;
    }

    long a = add(3, 4);           /* 7 */
    long m = mul(5, 6);           /* 30 */
    long n = neg(10);             /* -10 */
    long s = square(7);           /* 49 */
    dlclose(h);
    if (a == 7 && m == 30 && n == -10 && s == 49) return 0;
    fprintf(stderr, "got add=%ld mul=%ld neg=%ld square=%ld\n", a, m, n, s);
    return 3;
}
