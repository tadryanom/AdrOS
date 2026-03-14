// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef _SYS_UN_H
#define _SYS_UN_H
struct sockaddr_un {
    unsigned short sun_family;
    char sun_path[108];
};
#endif
