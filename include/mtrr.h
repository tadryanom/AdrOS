#ifndef MTRR_H
#define MTRR_H

#include <stdint.h>

#define MTRR_TYPE_UC  0  /* Uncacheable */
#define MTRR_TYPE_WC  1  /* Write-Combining */
#define MTRR_TYPE_WT  4  /* Write-Through */
#define MTRR_TYPE_WP  5  /* Write-Protect */
#define MTRR_TYPE_WB  6  /* Write-Back */

void mtrr_init(void);
int  mtrr_set_range(uint64_t base, uint64_t size, uint8_t type);

#endif
