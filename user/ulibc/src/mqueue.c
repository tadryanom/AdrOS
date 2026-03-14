// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "mqueue.h"
#include "syscall.h"
#include "errno.h"

mqd_t mq_open(const char* name, int oflag, ...) {
    return (mqd_t)__syscall_ret(_syscall2(SYS_MQ_OPEN, (int)name, oflag));
}

int mq_close(mqd_t mqdes) {
    return __syscall_ret(_syscall1(SYS_MQ_CLOSE, mqdes));
}

int mq_send(mqd_t mqdes, const char* msg_ptr, size_t msg_len, unsigned int msg_prio) {
    (void)msg_prio;
    return __syscall_ret(_syscall3(SYS_MQ_SEND, mqdes, (int)msg_ptr, (int)msg_len));
}

int mq_receive(mqd_t mqdes, char* msg_ptr, size_t msg_len, unsigned int* msg_prio) {
    (void)msg_prio;
    return __syscall_ret(_syscall3(SYS_MQ_RECEIVE, mqdes, (int)msg_ptr, (int)msg_len));
}

int mq_unlink(const char* name) {
    return __syscall_ret(_syscall1(SYS_MQ_UNLINK, (int)name));
}

int mq_getattr(mqd_t mqdes, struct mq_attr* attr) {
    (void)mqdes;
    (void)attr;
    errno = ENOSYS;
    return -1;
}

int mq_setattr(mqd_t mqdes, const struct mq_attr* newattr, struct mq_attr* oldattr) {
    (void)mqdes;
    (void)newattr;
    (void)oldattr;
    errno = ENOSYS;
    return -1;
}
