#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdint.h>

size_t strlen(const char* str);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
char* strcpy(char* dest, const char* src);
void* memset(void* ptr, int value, size_t num);
void* memcpy(void* dst, const void* src, size_t n);

// Reverse a string (helper for itoa)
void reverse(char* str, int length);
// Integer to ASCII
void itoa(int num, char* str, int base);
// ASCII to Integer
int atoi(const char* str);
// Hex dumper
void itoa_hex(uint32_t num, char* str);

#endif
