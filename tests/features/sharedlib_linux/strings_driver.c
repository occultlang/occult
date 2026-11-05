#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

typedef const char* (*strfn)(void);
typedef long (*numfn)(void);

int main(int argc, char** argv) {
    const char* so = argc > 1 ? argv[1] : "./libstrings.so";
    void* h = dlopen(so, RTLD_NOW);
    if (!h) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }

    strfn g = (strfn) dlsym(h, "greet");
    numfn n = (numfn) dlsym(h, "get_num");
    if (!g || !n) { fprintf(stderr, "dlsym\n"); return 2; }

    /* Occult strings have the length stored 8 bytes before the pointer. */
    const char* s = g();
    long len;
    memcpy(&len, s - 8, sizeof(len));
    const char* expected = "Hello from Occult shared library";
    long elen = (long) strlen(expected);
    long num = n();
    int ok = (len == elen && num == 42 && memcmp(s, expected, (size_t)elen) == 0);
    dlclose(h);
    if (ok) return 0;
    fprintf(stderr, "got len=%ld want=%ld num=%ld\n", len, elen, num);
    return 3;
}
