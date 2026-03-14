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
#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include <stddef.h>

/* /etc/passwd and /etc/group parsing with static fallback.
 * Format: name:passwd:uid:gid:gecos:dir:shell */

static struct passwd _root = {
    .pw_name   = "root",
    .pw_passwd = "x",
    .pw_uid    = 0,
    .pw_gid    = 0,
    .pw_gecos  = "root",
    .pw_dir    = "/",
    .pw_shell  = "/bin/sh",
};

static struct passwd _parsed_pw;
static char _pw_line[256];
static FILE* _pw_fp = (void*)0;
static int _pw_idx = 0;

static struct passwd* parse_passwd_line(char* line) {
    char* fields[7];
    int n = 0;
    char* p = line;
    fields[n++] = p;
    while (*p && n < 7) {
        if (*p == ':') { *p = '\0'; fields[n++] = p + 1; }
        p++;
    }
    if (n < 7) return (struct passwd*)0;
    _parsed_pw.pw_name   = fields[0];
    _parsed_pw.pw_passwd = fields[1];
    _parsed_pw.pw_uid    = atoi(fields[2]);
    _parsed_pw.pw_gid    = atoi(fields[3]);
    _parsed_pw.pw_gecos  = fields[4];
    _parsed_pw.pw_dir    = fields[5];
    _parsed_pw.pw_shell  = fields[6];
    /* Strip trailing newline */
    size_t sl = strlen(_parsed_pw.pw_shell);
    if (sl > 0 && _parsed_pw.pw_shell[sl - 1] == '\n')
        _parsed_pw.pw_shell[sl - 1] = '\0';
    return &_parsed_pw;
}

struct passwd* getpwnam(const char* name) {
    if (!name) return (struct passwd*)0;
    FILE* fp = fopen("/etc/passwd", "r");
    if (fp) {
        while (fgets(_pw_line, (int)sizeof(_pw_line), fp)) {
            struct passwd* pw = parse_passwd_line(_pw_line);
            if (pw && strcmp(pw->pw_name, name) == 0) {
                fclose(fp);
                return pw;
            }
        }
        fclose(fp);
    }
    if (strcmp(name, "root") == 0) return &_root;
    return (struct passwd*)0;
}

struct passwd* getpwuid(int uid) {
    FILE* fp = fopen("/etc/passwd", "r");
    if (fp) {
        while (fgets(_pw_line, (int)sizeof(_pw_line), fp)) {
            struct passwd* pw = parse_passwd_line(_pw_line);
            if (pw && pw->pw_uid == uid) {
                fclose(fp);
                return pw;
            }
        }
        fclose(fp);
    }
    if (uid == 0) return &_root;
    return (struct passwd*)0;
}

void setpwent(void) {
    if (_pw_fp) fclose(_pw_fp);
    _pw_fp = fopen("/etc/passwd", "r");
    _pw_idx = 0;
}

void endpwent(void) {
    if (_pw_fp) { fclose(_pw_fp); _pw_fp = (void*)0; }
    _pw_idx = 0;
}

struct passwd* getpwent(void) {
    if (_pw_fp) {
        if (fgets(_pw_line, (int)sizeof(_pw_line), _pw_fp))
            return parse_passwd_line(_pw_line);
        return (struct passwd*)0;
    }
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

static struct group _parsed_gr;
static char _gr_line[256];
static char* _gr_members[16];
static FILE* _gr_fp = (void*)0;
static int _gr_idx = 0;

static struct group* parse_group_line(char* line) {
    /* Format: name:passwd:gid:member1,member2,... */
    char* fields[4];
    int n = 0;
    char* p = line;
    fields[n++] = p;
    while (*p && n < 4) {
        if (*p == ':') { *p = '\0'; fields[n++] = p + 1; }
        p++;
    }
    if (n < 3) return (struct group*)0;
    _parsed_gr.gr_name   = fields[0];
    _parsed_gr.gr_passwd = (n >= 2) ? fields[1] : "x";
    _parsed_gr.gr_gid    = atoi(fields[2]);
    /* Parse members */
    int mi = 0;
    if (n >= 4 && fields[3][0] != '\0' && fields[3][0] != '\n') {
        char* mp = fields[3];
        _gr_members[mi++] = mp;
        while (*mp && mi < 15) {
            if (*mp == ',' || *mp == '\n') { *mp = '\0'; _gr_members[mi++] = mp + 1; }
            mp++;
        }
    }
    _gr_members[mi] = (char*)0;
    _parsed_gr.gr_mem = _gr_members;
    return &_parsed_gr;
}

struct group* getgrnam(const char* name) {
    if (!name) return (struct group*)0;
    FILE* fp = fopen("/etc/group", "r");
    if (fp) {
        while (fgets(_gr_line, (int)sizeof(_gr_line), fp)) {
            struct group* gr = parse_group_line(_gr_line);
            if (gr && strcmp(gr->gr_name, name) == 0) {
                fclose(fp);
                return gr;
            }
        }
        fclose(fp);
    }
    if (strcmp(name, "root") == 0) return &_root_grp;
    return (struct group*)0;
}

struct group* getgrgid(int gid) {
    FILE* fp = fopen("/etc/group", "r");
    if (fp) {
        while (fgets(_gr_line, (int)sizeof(_gr_line), fp)) {
            struct group* gr = parse_group_line(_gr_line);
            if (gr && gr->gr_gid == gid) {
                fclose(fp);
                return gr;
            }
        }
        fclose(fp);
    }
    if (gid == 0) return &_root_grp;
    return (struct group*)0;
}

void setgrent(void) {
    if (_gr_fp) fclose(_gr_fp);
    _gr_fp = fopen("/etc/group", "r");
    _gr_idx = 0;
}

void endgrent(void) {
    if (_gr_fp) { fclose(_gr_fp); _gr_fp = (void*)0; }
    _gr_idx = 0;
}

struct group* getgrent(void) {
    if (_gr_fp) {
        if (fgets(_gr_line, (int)sizeof(_gr_line), _gr_fp))
            return parse_group_line(_gr_line);
        return (struct group*)0;
    }
    if (_gr_idx == 0) { _gr_idx++; return &_root_grp; }
    return (struct group*)0;
}
