// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef ULIBC_MQUEUE_H
#define ULIBC_MQUEUE_H

#include <stddef.h>
#include <stdint.h>

typedef int mqd_t;

struct mq_attr {
    long mq_flags;
    long mq_maxmsg;
    long mq_msgsize;
    long mq_curmsgs;
};

mqd_t mq_open(const char* name, int oflag, ...);
int   mq_close(mqd_t mqdes);
int   mq_send(mqd_t mqdes, const char* msg_ptr, size_t msg_len, unsigned int msg_prio);
int   mq_receive(mqd_t mqdes, char* msg_ptr, size_t msg_len, unsigned int* msg_prio);
int   mq_unlink(const char* name);
int   mq_getattr(mqd_t mqdes, struct mq_attr* attr);
int   mq_setattr(mqd_t mqdes, const struct mq_attr* newattr, struct mq_attr* oldattr);

#endif
