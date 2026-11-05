#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef const char* (*strfn)(void);
typedef long long (*numfn)(void);

int main(int argc, char** argv) {
    const char* dll = argc > 1 ? argv[1] : "strings.dll";
    HMODULE h = LoadLibraryA(dll);
    if (!h) { fprintf(stderr, "LoadLibrary(%s) failed: %lu\n", dll, GetLastError()); return 1; }

    strfn g = (strfn) GetProcAddress(h, "greet");
    numfn n = (numfn) GetProcAddress(h, "get_num");
    if (!g || !n) { fprintf(stderr, "GetProcAddress\n"); return 2; }

    const char* s = g();
    long long len;
    memcpy(&len, s - 8, sizeof(len));
    const char* expected = "Hello from Occult shared library";
    long long elen = (long long) strlen(expected);
    long long num = n();
    int ok = (len == elen && num == 42 && memcmp(s, expected, (size_t)elen) == 0);
    FreeLibrary(h);
    if (ok) return 0;
    fprintf(stderr, "got len=%lld want=%lld num=%lld\n", len, elen, num);
    return 3;
}
