#!/bin/sh
#
#	$OpenBSD: install.md,v 1.27 2002/04/28 14:44:01 krw Exp $
#	$NetBSD: install.md,v 1.1.2.4 1996/08/26 15:45:14 gwr Exp $
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
# machine dependent section of installation/upgrade script
#

# Machine-dependent install sets
MDSETS=kernel
MDTERM=hp300h
ARCH=ARCH

md_set_term() {
}

md_get_diskdevs() {
	# return available disk devices
	dmesg | egrep -a "^hd[0-9]*:." | cutword -t: 1 | sort -u
	dmesg | egrep -a "^sd[0-9]*:.*cylinders" | cutword -t: 1 | sort -u
}

md_get_cddevs() {
	# return available CD-ROM devices
	dmesg | egrep -a "sd[0-9]*:.*CD-ROM" | cutword -t: 1 | sort -u
}

md_questions() {
	:
}

md_installboot() {
	# $1 is the root disk

	echo -n "Installing boot block..."
	disklabel -W ${1}
	disklabel -B ${1}
	echo "done."
}

md_checkfordisklabel() {
	# $1 is the disk to check
	local rval

	disklabel -r $1 > /dev/null 2> /tmp/checkfordisklabel
	if grep "no disk label" /tmp/checkfordisklabel; then
		rval=1
	elif grep "disk label corrupted" /tmp/checkfordisklabel; then
		rval=2
	else
		rval=0
	fi

	rm -f /tmp/checkfordisklabel
	return $rval
}

md_prep_disklabel()
{
	local _disk

	_disk=$1
	md_checkfordisklabel $_disk
	case $? in
	0)	;;
	1)	echo "WARNING: Disk $_disk has no label. You will be creating a new one."
		echo
		;;
	2)	echo "WARNING: Label on disk $_disk is corrupted. You will be repairing."
		echo
		;;
	esac

	# display example
	cat << __EOT

If you are unsure of how to use multiple partitions properly
(ie. separating /, /usr, /tmp, /var, /usr/local, and other things)
just split the space into a root and swap partition for now.
__EOT

	disklabel -W ${_disk}
	disklabel -f /tmp/fstab.${_disk} -E ${_disk}
}

# Note, while they might not seem machine-dependent, the
# welcome banner and the punt message may contain information
# and/or instructions specific to the type of machine.

md_welcome_banner() {
(
	if [ "$MODE" = "install" ]; then
		echo "Welcome to the OpenBSD/hp300 ${VERSION_MAJOR}.${VERSION_MINOR} installation program."
		cat << __EOT

This program is designed to help you put OpenBSD on your system in a
simple and rational way.
__EOT

	else
		echo "Welcome to the OpenBSD/hp300 ${VERSION_MAJOR}.${VERSION_MINOR} upgrade program."
		cat << __EOT

This program is designed to help you upgrade your OpenBSD system in a
simple and rational way.

As a reminder, installing the `etc' binary set is NOT recommended.
Once the rest of your system has been upgraded, you should manually
merge any changes to files in the `etc' set into those files which
already exist on your system.
__EOT
	fi

cat << __EOT

As with anything which modifies your disk's contents, this program can
cause SIGNIFICANT data loss, and you are advised to make sure your
data is backed up before beginning the installation process.

Default answers are displayed in brackets after the questions.  You
can hit Control-C at any time to quit, but if you do so at a prompt,
you may have to hit return.  Also, quitting in the middle of
installation may leave your system in an inconsistent state.

__EOT
) | less -E
}

md_not_going_to_install() {
		cat << __EOT

OK, then.  Enter 'halt' at the prompt to halt the machine.  Once the
machine has halted, power-cycle the system to load new boot code.

__EOT
}

md_congrats() {
	cat << __EOT

CONGRATULATIONS!  You have successfully installed OpenBSD!  To boot the
installed system, enter halt at the command prompt.  Once the system has
halted, power-cycle the machine in order to load new boot code.  Make sure
you boot from the root disk.

__EOT
}

md_native_fstype() {
	# Nothing to do.
}

md_native_fsopts() {
	# Nothing to do.
}
