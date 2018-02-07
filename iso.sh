#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
#
# AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
# All rights reserved.
# See LICENSE for details.
#
# Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
# Mirror: https://github.com/tadryanom/AdrOS
#

set -e
. ./build.sh

mkdir -p isodir
mkdir -p isodir/boot
mkdir -p isodir/boot/grub

cp sysroot/boot/myos.kernel isodir/boot/myos.kernel
cat > isodir/boot/grub/grub.cfg << EOF
menuentry "myos" {
	multiboot /boot/myos.kernel
}
EOF
grub-mkrescue -o myos.iso isodir
