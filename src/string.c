#include <string.h>

u8int *memcpy (u8int *dest, const u8int *src, s32int count)
{
    const s8int *sp = (const s8int *) src;
    s8int *dp = (s8int *) dest;
    for (; count != 0; count--)
        *dp++ = *sp++;
    return dest;
}

u8int *memset (u8int *dest, u8int val, s32int count)
{
    s8int *tmp = (s8int *) dest;
    for ( ; count != 0; count--)
        *tmp++ = val;
    return dest;
}

u16int *memsetw (u16int *dest, u16int val, s32int count)
{
    u16int *tmp = (u16int *) dest;
    for ( ; count != 0; count--)
        *tmp++ = val;
    return dest;
}

s32int strlen (const s8int *str)
{
    size_t ret;
    for (ret = 0; *str != '\0'; str++)
        ret++;
    return ret;
}
