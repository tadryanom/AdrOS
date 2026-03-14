// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "locale.h"
#include <stddef.h>

static struct lconv _c_lconv = {
    .decimal_point    = ".",
    .thousands_sep    = "",
    .grouping         = "",
    .int_curr_symbol  = "",
    .currency_symbol  = "",
    .mon_decimal_point = "",
    .mon_thousands_sep = "",
    .mon_grouping     = "",
    .positive_sign    = "",
    .negative_sign    = "",
    .int_frac_digits  = 127,
    .frac_digits      = 127,
    .p_cs_precedes    = 127,
    .p_sep_by_space   = 127,
    .n_cs_precedes    = 127,
    .n_sep_by_space   = 127,
    .p_sign_posn      = 127,
    .n_sign_posn      = 127,
};

char* setlocale(int category, const char* locale) {
    (void)category;
    (void)locale;
    return "C";
}

struct lconv* localeconv(void) {
    return &_c_lconv;
}
