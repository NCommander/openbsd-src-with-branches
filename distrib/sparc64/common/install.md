#	$OpenBSD: install.md,v 1.33 2009/06/04 00:44:47 krw Exp $
#	$NetBSD: install.md,v 1.3.2.5 1996/08/26 15:45:28 gwr Exp $
#
#
# Copyright (c) 1996 The NetBSD Foundation, Inc.
# All rights reserved.
#
# This code is derived from software contributed to The NetBSD Foundation
# by Jason R. Thorpe.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
#
# machine dependent section of installation/upgrade script.
#

MDTERM=sun
MDXAPERTURE=1
MDXDM=y
NCPU=$(sysctl -n hw.ncpufound)

((NCPU > 1)) && { DEFAULTSETS="bsd bsd.rd bsd.mp" ; SANESETS="bsd bsd.mp" ; }

md_installboot() {
	local _prefix

	if [[ -f /mnt/bsd.mp ]] && ((NCPU > 1)); then
		echo "Multiprocessor machine; using bsd.mp instead of bsd."
		mv /mnt/bsd /mnt/bsd.sp 2>/dev/null
		mv /mnt/bsd.mp /mnt/bsd
	fi

	# Figure out which /usr/mdec directory to use
	if [ -e /mnt/usr/mdec/ofwboot ]; then
		_prefix=/mnt/usr/mdec
	elif [ -e /usr/mdec/ofwboot ]; then
		_prefix=/usr/mdec
	else
		echo No boot block prototypes found, you must run installboot manually.
		return
	fi

	cp ${_prefix}/ofwboot /mnt/ofwboot
	sync
	${_prefix}/installboot ${_prefix}/bootblk /dev/r${1}c
}

md_prep_disklabel() {
	local _disk=$1 _f _op

	disklabel -W $_disk >/dev/null 2>&1
	_f=/tmp/fstab.$_disk
	if [[ $_disk == $ROOTDISK ]]; then
		while :; do
			echo "The auto-allocated layout for $_disk is:"
			disklabel -h -A $_disk | egrep "^#  |^  [a-p]:"
			ask "Use (A)uto layout, (E)dit auto layout, or create (C)ustom layout?" a
			case $resp in
			a*|A*)	_op=-w ; AUTOROOT=y ;;
			e*|E*)	_op=-E ;;
			c*|C*)	break ;;
			*)	continue ;;
			esac
			disklabel -f $_f $_op -A $_disk
			return
		done
	fi
	cat <<__EOT

You will now create a Sun-style disklabel on the disk.  The disklabel defines
how OpenBSD splits up the disk into OpenBSD partitions in which filesystems
and swap space are created.  You must provide each filesystem's mountpoint
in this program.

__EOT

	disklabel -f $_f -E $_disk
}

md_congrats() {
}

md_consoleinfo() {
}
