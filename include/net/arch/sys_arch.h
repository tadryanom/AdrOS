#ifndef LWIP_ARCH_SYS_ARCH_H
#define LWIP_ARCH_SYS_ARCH_H

#include "sync.h"
#include <stdint.h>

/* lwIP sys_arch types backed by AdrOS kernel primitives */

typedef ksem_t*   sys_sem_t;
typedef kmutex_t* sys_mutex_t;
typedef kmbox_t*  sys_mbox_t;
typedef void*     sys_thread_t;
typedef uintptr_t sys_prot_t;

#define sys_sem_valid(s)           ((s) != NULL && *(s) != NULL)
#define sys_sem_valid_val(s)       ((s) != NULL)
#define sys_sem_set_invalid(s)     do { if (s) *(s) = NULL; } while(0)

#define sys_mutex_valid(m)         ((m) != NULL && *(m) != NULL)
#define sys_mutex_valid_val(m)     ((m) != NULL)
#define sys_mutex_set_invalid(m)   do { if (m) *(m) = NULL; } while(0)

#define sys_mbox_valid(mb)         ((mb) != NULL && *(mb) != NULL)
#define sys_mbox_valid_val(mb)     ((mb) != NULL)
#define sys_mbox_set_invalid(mb)   do { if (mb) *(mb) = NULL; } while(0)

#endif /* LWIP_ARCH_SYS_ARCH_H */
