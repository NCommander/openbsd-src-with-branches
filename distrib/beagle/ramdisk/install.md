#	$OpenBSD: install.md,v 1.13 2009/06/11 04:28:50 deraadt Exp $
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

md_installboot() {
}

md_prep_fdisk() {
	local _disk=$1 _q _d

	while :; do
		_d=whole
		if [[ -n $(fdisk $_disk | grep 'Signature: 0xAA55') ]]; then
			fdisk $_disk
			if [[ -n $(fdisk $_disk | grep '^..: A6 ') ]]; then
				_q=", use the (O)penBSD area,"
				_d=OpenBSD
			fi
		else
			echo "MBR has invalid signature; not showing it."
		fi
		ask "Use (W)hole disk$_q or (E)dit the MBR?" "$_d"
		case $resp in
		w*|W*)
			echo -n "Setting OpenBSD MBR partition to whole $_disk..."
			fdisk -e ${_disk} <<__EOT >/dev/null
reinit
update
write
quit
__EOT
			echo "done."
			return ;;
		e*|E*)
			# Manually configure the MBR.
			cat <<__EOT

You will now create a single MBR partition to contain your OpenBSD data. This
partition must have an id of 'A6'; must *NOT* overlap other partitions; and
must be marked as the only active partition.  Inside the fdisk command, the
'manual' command describes all the fdisk commands in detail.

$(fdisk ${_disk})
__EOT
			fdisk -e ${_disk}
			[[ -n $(fdisk $_disk | grep ' A6 ') ]] && return
			echo No OpenBSD partition in MBR, try again. ;;
		o*|O*)	return ;;
		esac
	done
}

md_prep_disklabel() {
	local _disk=$1 _f _op

	md_prep_fdisk $_disk

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

You will now create an OpenBSD disklabel inside the OpenBSD MBR
partition. The disklabel defines how OpenBSD splits up the MBR partition
into OpenBSD partitions in which filesystems and swap space are created.
You must provide each filesystem's mountpoint in this program.

The offsets used in the disklabel are ABSOLUTE, i.e. relative to the
start of the disk, NOT the start of the OpenBSD MBR partition.

__EOT

	disklabel -f $_f -E $_disk
}

md_congrats() {
}

md_consoleinfo() {
}
