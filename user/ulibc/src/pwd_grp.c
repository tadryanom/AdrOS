// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "pwd.h"
#include "grp.h"
#include <stddef.h>

/* Minimal /etc/passwd and /etc/group stubs.
 * AdrOS has a single-user model (uid=0 root). */

static struct passwd _root = {
    .pw_name   = "root",
    .pw_passwd = "x",
    .pw_uid    = 0,
    .pw_gid    = 0,
    .pw_gecos  = "root",
    .pw_dir    = "/",
    .pw_shell  = "/bin/sh",
};

static int _pw_idx = 0;

struct passwd* getpwnam(const char* name) {
    if (!name) return (struct passwd*)0;
    /* strcmp inline to avoid header dependency issues */
    const char* a = name;
    const char* b = "root";
    while (*a && *a == *b) { a++; b++; }
    if (*a == *b) return &_root;
    return (struct passwd*)0;
}

struct passwd* getpwuid(int uid) {
    if (uid == 0) return &_root;
    return (struct passwd*)0;
}

void setpwent(void) { _pw_idx = 0; }
void endpwent(void) { _pw_idx = 0; }

struct passwd* getpwent(void) {
    if (_pw_idx == 0) { _pw_idx++; return &_root; }
    return (struct passwd*)0;
}

static char* _root_members[] = { "root", (char*)0 };

static struct group _root_grp = {
    .gr_name   = "root",
    .gr_passwd = "x",
    .gr_gid    = 0,
    .gr_mem    = _root_members,
};

static int _gr_idx = 0;

struct group* getgrnam(const char* name) {
    if (!name) return (struct group*)0;
    const char* a = name;
    const char* b = "root";
    while (*a && *a == *b) { a++; b++; }
    if (*a == *b) return &_root_grp;
    return (struct group*)0;
}

struct group* getgrgid(int gid) {
    if (gid == 0) return &_root_grp;
    return (struct group*)0;
}

void setgrent(void) { _gr_idx = 0; }
void endgrent(void) { _gr_idx = 0; }

struct group* getgrent(void) {
    if (_gr_idx == 0) { _gr_idx++; return &_root_grp; }
    return (struct group*)0;
}
