#       $OpenBSD: install.md,v 1.19 2005/03/27 15:13:49 krw Exp $
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
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#        This product includes software developed by the NetBSD
#        Foundation, Inc. and its contributors.
# 4. Neither the name of The NetBSD Foundation nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
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

MDXAPERTURE=1
MDSERIAL="com com tty0"
ARCH=ARCH

md_installboot() {
	local _rawdev=/dev/r${1}c

	# use extracted mdec if it exists (may be newer)
	if [ -d /mnt/usr/mdec ]; then
		cp /mnt/usr/mdec/boot /mnt/boot
		/mnt/usr/mdec/installboot -v /mnt/boot /mnt/usr/mdec/bootxx $_rawdev
	elif [ -d /usr/mdec ]; then
		cp /usr/mdec/boot /mnt/boot
		/usr/mdec/installboot -v /mnt/boot /usr/mdec/bootxx $_rawdev
	else
		echo No boot block prototypes found, you must run installboot manually.
	fi
}

# $1 is the disk to check
md_checkfordisklabel() {
	local rval=0

	disklabel $1 >/dev/null 2>/tmp/checkfordisklabel

	if grep "disk label corrupted" /tmp/checkfordisklabel; then
		rval=2
	fi >/dev/null 2>&1

	rm -f /tmp/checkfordisklabel
	return $rval
}

md_prep_disklabel() {
	local _disk=$1

	md_checkfordisklabel $_disk
	case $? in
	2)	echo "WARNING: Label on disk $_disk is corrupted. You will be repairing it.\n"
		;;
	esac

	disklabel -W $_disk >/dev/null 2>&1
	disklabel -f /tmp/fstab.$_disk -E $_disk
}

md_congrats() {
}
