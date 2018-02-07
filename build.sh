#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
# All rights reserved.
# See LICENSE for details.
#
# Source: https://github.com/tadryanom/AdrOS
#

set -e
. ./headers.sh

for PROJECT in $PROJECTS; do
  (cd $PROJECT && DESTDIR="$SYSROOT" $MAKE install)
done
