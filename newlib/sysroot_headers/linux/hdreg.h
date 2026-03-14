// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef _LINUX_HDREG_H
#define _LINUX_HDREG_H
struct hd_driveid { unsigned short config; /* empty stub */ };
#define HDIO_GET_IDENTITY 0x030D
#endif
