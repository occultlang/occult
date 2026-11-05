#include <windows.h>
#include <stdio.h>

typedef long long (*binop)(long long, long long);
typedef long long (*unop)(long long);

int main(int argc, char** argv) {
    const char* dll = argc > 1 ? argv[1] : "binops.dll";
    HMODULE h = LoadLibraryA(dll);
    if (!h) { fprintf(stderr, "LoadLibrary(%s) failed: %lu\n", dll, GetLastError()); return 1; }

    binop add = (binop) GetProcAddress(h, "add");
    binop mul = (binop) GetProcAddress(h, "mul");
    unop  neg = (unop)  GetProcAddress(h, "neg");
    unop  sq  = (unop)  GetProcAddress(h, "square");
    if (!add || !mul || !neg || !sq) {
        fprintf(stderr, "missing symbol\n");
        return 2;
    }

    long long a = add(3, 4);           /* 7 */
    long long m = mul(5, 6);           /* 30 */
    long long n = neg(10);             /* -10 */
    long long s = sq(7);               /* 49 */
    FreeLibrary(h);
    if (a == 7 && m == 30 && n == -10 && s == 49) return 0;
    fprintf(stderr, "got add=%lld mul=%lld neg=%lld square=%lld\n", a, m, n, s);
    return 3;
}
