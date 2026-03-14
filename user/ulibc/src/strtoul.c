// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "stdlib.h"
#include "ctype.h"
#include "errno.h"
#include <stdint.h>

unsigned long strtoul(const char* nptr, char** endptr, int base) {
    const char* s = nptr;
    unsigned long result = 0;
    int neg = 0;

    while (isspace(*s)) s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') { s++; }

    if (base == 0) {
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { base = 16; s += 2; }
        else if (s[0] == '0') { base = 8; s++; }
        else { base = 10; }
    } else if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
    }

    int overflow = 0;
    while (*s) {
        int digit;
        if (*s >= '0' && *s <= '9') digit = *s - '0';
        else if (*s >= 'a' && *s <= 'z') digit = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'Z') digit = *s - 'A' + 10;
        else break;
        if (digit >= base) break;
        if (result > (0xFFFFFFFFUL - (unsigned long)digit) / (unsigned long)base)
            overflow = 1;
        result = result * (unsigned long)base + (unsigned long)digit;
        s++;
    }

    if (endptr) *endptr = (char*)s;
    if (overflow) { errno = ERANGE; return 0xFFFFFFFFUL; }
    return neg ? (unsigned long)(-(long)result) : result;
}

long long strtoll(const char* nptr, char** endptr, int base) {
    const char* s = nptr;
    long long result = 0;
    int neg = 0;

    while (isspace(*s)) s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') { s++; }

    if (base == 0) {
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { base = 16; s += 2; }
        else if (s[0] == '0') { base = 8; s++; }
        else { base = 10; }
    } else if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
    }

    while (*s) {
        int digit;
        if (*s >= '0' && *s <= '9') digit = *s - '0';
        else if (*s >= 'a' && *s <= 'z') digit = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'Z') digit = *s - 'A' + 10;
        else break;
        if (digit >= base) break;
        result = result * base + digit;
        s++;
    }

    if (endptr) *endptr = (char*)s;
    return neg ? -result : result;
}

unsigned long long strtoull(const char* nptr, char** endptr, int base) {
    const char* s = nptr;
    unsigned long long result = 0;
    int neg = 0;

    while (isspace(*s)) s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') { s++; }

    if (base == 0) {
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { base = 16; s += 2; }
        else if (s[0] == '0') { base = 8; s++; }
        else { base = 10; }
    } else if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
    }

    while (*s) {
        int digit;
        if (*s >= '0' && *s <= '9') digit = *s - '0';
        else if (*s >= 'a' && *s <= 'z') digit = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'Z') digit = *s - 'A' + 10;
        else break;
        if (digit >= base) break;
        result = result * (unsigned long long)base + (unsigned long long)digit;
        s++;
    }

    if (endptr) *endptr = (char*)s;
    return neg ? (unsigned long long)(-(long long)result) : result;
}
