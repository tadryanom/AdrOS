#include <stdlib.h>

/* 
 * Convert the integer D to a string and save the string in BUF. If
 * BASE is equal to ’d’, interpret that D is decimal, and if BASE is
 * equal to ’x’, interpret that D is hexadecimal.
 */
//void itoa (s8int *buf, s32int base, s32int d)
s8int * itoa (s32int d, s8int *buf, s32int base)
{
    s8int *p = buf;
    s8int *p1, *p2;
    u64int ud = d;
    s32int divisor = 10;
  
    // If %d is specified and D is minus, put ‘-’ in the head.
    if (base == 'd' && d < 0) {
        *p++ = '-';
        buf++;
        ud = -d;
    } else if (base == 'x')
        divisor = 16;

    // Divide UD by DIVISOR until UD == 0.
    do {
        s32int remainder = ud % divisor;
        *p++ = (remainder < 10) ? remainder + '0' : remainder + 'a' - 10;
    } while (ud /= divisor);

    // Terminate BUF.
    *p = 0;
    // Reverse BUF.
    p1 = buf;
    p2 = p - 1;
    while (p1 < p2) {
        s8int tmp = *p1;
        *p1 = *p2;
        *p2 = tmp;
        p1++;
        p2--;
    }
    return buf;
}
