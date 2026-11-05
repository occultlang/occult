/* Linked against exports_to_c.o; this file holds main. */
#include <stdio.h>

long occult_add(long a, long b);
long occult_sum_to(long n);
extern long table; /* an Occult global */

int main(void) {
    if (table != 7) {
        printf("table = %ld, expected 7 (did .init_array run?)\n", table);
        return 1;
    }
    if (occult_add(1, 2) != 10) {
        printf("occult_add(1, 2) = %ld, expected 10\n", occult_add(1, 2));
        return 1;
    }
    table = 100;
    if (occult_add(1, 2) != 103) {
        puts("Occult did not see C's write to the global");
        return 1;
    }
    if (occult_sum_to(100) != 5050) {
        printf("occult_sum_to(100) = %ld\n", occult_sum_to(100));
        return 1;
    }
    puts("PASS");
    return 0;
}
