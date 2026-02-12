#ifndef ULIBC_PTHREAD_H
#define ULIBC_PTHREAD_H

#include <stdint.h>
#include <stddef.h>

typedef uint32_t pthread_t;

typedef struct {
    size_t stack_size;
    int    detach_state;
} pthread_attr_t;

#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1

/* Create a new thread.
 * thread: output thread ID
 * attr: thread attributes (may be NULL for defaults)
 * start_routine: function pointer
 * arg: argument passed to start_routine
 * Returns 0 on success, errno on failure.
 */
int pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                   void* (*start_routine)(void*), void* arg);

/* Wait for a thread to finish.
 * thread: thread ID to wait for
 * retval: if non-NULL, stores the thread's return value
 * Returns 0 on success, errno on failure.
 */
int pthread_join(pthread_t thread, void** retval);

/* Terminate the calling thread.
 * retval: return value for pthread_join
 */
void pthread_exit(void* retval) __attribute__((noreturn));

/* Return the calling thread's ID. */
pthread_t pthread_self(void);

/* Initialize thread attributes to defaults. */
int pthread_attr_init(pthread_attr_t* attr);

/* Destroy thread attributes. */
int pthread_attr_destroy(pthread_attr_t* attr);

/* Set stack size in thread attributes. */
int pthread_attr_setstacksize(pthread_attr_t* attr, size_t stacksize);

#endif
