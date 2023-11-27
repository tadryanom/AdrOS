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

s32int strcmp (s8int *str1, s8int *str2)
{
    s32int i = 0;
    s32int failed = 0;
    while(str1[i] != '\0' && str2[i] != '\0') {
        if(str1[i] != str2[i]) {
            failed = 1;
            break;
        }
        i++;
    }
    // why did the loop exit?
    if( (str1[i] == '\0' && str2[i] != '\0') || (str1[i] != '\0' && str2[i] == '\0') )
        failed = 1;
  
    return failed;
}

s8int *strcpy(s8int *dest, const s8int *src)
{
    do {
        *dest++ = *src++;
    }
    while (*src != 0);
    return dest;
}

s8int *strcat (s8int *dest, const s8int *src)
{
    volatile s8int *tmp = 0;
    while (*dest != 0) {
        *tmp = *dest++;
        //*dest = *dest++;
        *dest = *tmp;
    }

    do {
        *dest++ = *src++;
    }
    while (*src != 0);
    return dest;
}
