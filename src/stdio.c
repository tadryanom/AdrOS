#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <screen.h>

/* Put the character */
void putchar (s8int c)
{
    put_char(c);
}

/* Print a string on screen */
void puts(s8int *text)
{
    for (s32int i = 0; i < strlen((const s8int*)text); i++)
        putchar(text[i]);
}

/* Format a string and print */
void printf (const s8int *format, ...)
{
    s8int **arg = (s8int **) &format;
    s32int c;
    s8int buf[20];

    arg++;
    while ((c = *format++) != 0) {
        if (c != '%')
            putchar (c);
        else {
            s8int *p, *p2;
            s32int pad0 = 0, pad = 0;
            c = *format++;
            if (c == '0') {
                pad0 = 1;
                c = *format++;
            }
            if (c >= '0' && c <= '9') {
                pad = c - '0';
                c = *format++;
            }
            switch (c) {
                case 'd':
                case 'u':
                case 'x':
                    itoa (buf, c, *((s32int *) arg++));
                    p = buf;
                    goto string;
                    break;
                case 's':
                    p = *arg++;
                    if (! p)
                        p = "(null)";
                string:
                    for (p2 = p; *p2; p2++){}
                    for (; p2 < p + pad; p2++)
                        putchar (pad0 ? '0' : ' ');
                    while (*p)
                        putchar (*p++);
                    break;
                default:
                    putchar (*((s32int *) arg++));
                    break;
            }
        }
    }
}
