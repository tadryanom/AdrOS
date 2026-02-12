#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

#include <stdint.h>
#include <stddef.h>

/* Define byte order based on target architecture */
#if defined(__i386__) || defined(__x86_64__) || defined(__arm__) || defined(__aarch64__) || defined(__riscv)
#define BYTE_ORDER LITTLE_ENDIAN
#elif defined(__MIPSEB__) || defined(__ARMEB__)
#define BYTE_ORDER BIG_ENDIAN
#else
#define BYTE_ORDER LITTLE_ENDIAN
#endif

/* Use GCC-style struct packing */
#define PACK_STRUCT_FIELD(x) x
#define PACK_STRUCT_STRUCT __attribute__((packed))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END

/* Compiler hints */
#define LWIP_PLATFORM_DIAG(x)   do { } while(0)
#define LWIP_PLATFORM_ASSERT(x) do { } while(0)

/* Use our own printf-less approach; lwIP only needs these types */
typedef uint8_t  u8_t;
typedef int8_t   s8_t;
typedef uint16_t u16_t;
typedef int16_t  s16_t;
typedef uint32_t u32_t;
typedef int32_t  s32_t;
typedef uintptr_t mem_ptr_t;

/* Provide format macros for lwIP debug prints (unused with NO_SYS + no debug) */
#define U16_F "u"
#define S16_F "d"
#define X16_F "x"
#define U32_F "u"
#define S32_F "d"
#define X32_F "x"
#define SZT_F "u"

#endif /* LWIP_ARCH_CC_H */
