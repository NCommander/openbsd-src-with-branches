#       $OpenBSD: install.md,v 1.15 1997/10/30 21:14:41 niklas Exp $
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

TMPWRITEABLE=/tmp/writeable
KERNFSMOUNTED=/tmp/kernfsmounted

# Machine-dependent install sets
MDSETS="kernel"

md_machine_arch() {
	cat /kern/machine
}

md_copy_kernel() {
	if [ ! -s /mnt/bsd ]; then
		echo	""
		echo	"Warning, no kernel installed!"
		echo	"You did not unpack a file set containing a kernel."
		echo	"This is needed to boot.  Please note that the install"
		echo	"install kernel is not suitable for general use."
		echo -n	"Escape to shell add /mnt/bsd by hand? [y] "
		getresp "y"
		case "$resp" in
			y*|Y*)
				echo "Type 'exit' to return to install."
				sh
				;;
			*)
				;;
		esac
	fi
}

md_set_term() {
	if [ ! -z "$TERM" ]; then
		return
	fi
	echo -n "Specify terminal type [sun]: "
	getresp "sun"
	TERM="$resp"
	export TERM
}

md_get_msgbuf() {
	# Only want to see one boot's worth of info
	sed -n -f /dev/stdin /kern/msgbuf <<- OOF
		/^Copyright (c)/h
		/^Copyright (c)/!H
		\${
			g
			p
		}
	OOF
}

md_get_diskdevs() {
	md_get_msgbuf | egrep "^sd[0-9] "
}

md_get_cddevs() {
	md_get_msgbuf | egrep "^cd[0-9] "
}

md_get_ifdevs() {
        # return available network devices
	md_get_msgbuf | egrep "(^e[dp][0-9] |^[dl]e[0-9] |fxp[0-9])" | sed -e 's/^ *//' -e 's/ .*//'
}

md_get_partition_range() {
	# return range of valid partition letters
	echo "[a-p]"
}

md_installboot() {
	local _rawdev

	if [ "X${1}" = X"" ]; then
		echo "No disk device specified, you must run installboot manually."
		return
	fi
	_rawdev=/dev/r${1}c

	# use extracted mdec if it exists (may be newer)
	if [ -d /mnt/usr/mdec ]; then
		cp /mnt/usr/mdec/boot /mnt/boot
		/mnt/usr/mdec/installboot -v /mnt/boot /mnt/usr/mdec/bootxx $_rawdev
	elif [ -d /usr/mdec ]; then
		cp /usr/mdec/boot /mnt/boot
		/usr/mdec/installboot -v /mnt/boot /usr/mdec/bootxx $_rawdev
	else
		echo "No boot block prototypes found, you must run installboot manually."
	fi
}
md_labeldisk() {
	echo "huh"
}

md_checkfordisklabel() {
	# $1 is the disk to check
	local rval

	disklabel $1 >> /dev/null 2> /tmp/checkfordisklabel
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
	0)
		;;
	1)
		echo "WARNING: Disk $_disk has no label. You will be creating a new one."
		echo
		;;
	2)
		echo "WARNING: Label on disk $_disk is corrupted. You will be repairing."
		echo
		;;
	esac

	# display example
	cat << \__md_prep_disklabel_1

If you are unsure of how to use multiple partitions properly
(ie. seperating /, /usr, /tmp, /var, /usr/local, and other things)
just split the space into a root and swap partition for now.
__md_prep_disklabel_1

	disklabel -W ${_disk}
	disklabel -E ${_disk}
}

md_welcome_banner() {
{
	if [ "$MODE" = "install" ]; then
		echo ""
		echo "Welcome to the OpenBSD/alpha ${VERSION_MAJOR}.${VERSION_MINOR} installation program."
		cat << \__welcome_banner_1

This program is designed to help you put OpenBSD on your disk, in a
simple and rational way.  You'll be asked several questions, and it
would probably be useful to have your disk's hardware manual, the
installation notes, and a calculator handy.
__welcome_banner_1

	else
		echo ""
		echo "Welcome to the OpenBSD/alpha ${VERSION} upgrade program."
		cat << \__welcome_banner_2

This program is designed to help you upgrade your OpenBSD system in a
simple and rational way.

As a reminder, installing the `etc' binary set is NOT recommended.
Once the rest of your system has been upgraded, you should manually
merge any changes to files in the `etc' set into those files which
already exist on your system.

__welcome_banner_2
	fi

cat << \__welcome_banner_3

As with anything which modifies your disk's contents, this program can
cause SIGNIFICANT data loss, and you are advised to make sure your
data is backed up before beginning the installation process.

Default answers are displayed in brackets after the questions.  You
can hit Control-C at any time to quit, but if you do so at a prompt,
you may have to hit return.  Also, quitting in the middle of
installation may leave your system in an inconsistent state.

__welcome_banner_3
} | more
}

md_not_going_to_install() {
	cat << \__not_going_to_install_1

OK, then.  Enter `halt' at the prompt to halt the machine.  Once the
machine has halted, power-cycle the system to load new boot code.

__not_going_to_install_1
}

md_congrats() {
	local what;
	if [ "$MODE" = "install" ]; then
		what="installed";
	else
		what="upgraded";
	fi
	cat << __congratulations_1

CONGRATULATIONS!  You have successfully $what OpenBSD!
To boot the installed system, enter halt at the command prompt. Once the
system has halted, reset the machine and boot from the disk.

__congratulations_1
}

md_native_fstype() {
}
md_native_fsopts() {
}

md_makerootwritable() {
	# 2048 is the size in DEV_BIZE blocks

	if [ -e ${TMPWRITEABLE} ]
	then
		md_mountkernfs
		return
	fi
	umount /tmp >> /dev/null 2>&1
	if ! mount -t ffs  -u /dev/rd0a / ; then
		cat << \__rd0_failed_1

FATAL ERROR: Can't mount the ram filesystem.

__rd0_failed_1
		exit
	fi

	# Bleh.  Give mount_mfs a chance to DTRT.
	sleep 2
	> ${TMPWRITEABLE}

	md_mountkernfs
}
md_mountkernfs() {
	if [ -e ${KERNFSMOUNTED} ]
	then
		return
	fi
	if ! mount -t kernfs /kern /kern
	then
		cat << \__kernfs_failed_1
FATAL ERROR: Can't mount kernfs filesystem
__kernfs_failed_1
		exit
	fi
	> ${KERNFSMOUNTED}
}

hostname() {
	case $# in
		0)	cat /kern/hostname ;;
		1)	echo "$1" > /kern/hostname ;;
		*)	echo "usage: hostname [name-of-host]"
	esac
}
