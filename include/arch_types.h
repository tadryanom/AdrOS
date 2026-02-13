#ifndef ARCH_TYPES_H
#define ARCH_TYPES_H

/*
 * Architecture-specific type constants.
 * This header provides opaque sizing information so that generic kernel
 * headers (e.g. process.h) can embed arch data without including the
 * full arch register / interrupt definitions.
 */

#if defined(__i386__) || defined(__x86_64__)
#include "arch/x86/arch_types.h"
#else
/* Fallback for non-x86: define a generous default */
#define ARCH_REGS_SIZE 64
#endif

#endif /* ARCH_TYPES_H */
