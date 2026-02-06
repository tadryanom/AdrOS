// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

.section .text
.global tss_flush
tss_flush:
    mov 4(%esp), %ax
    ltr %ax
    ret

 .section .note.GNU-stack,"",@progbits
