#!/bin/sh
#	$OpenBSD: upgrade.sh,v 1.32 2002/04/28 14:44:01 krw Exp $
#	$NetBSD: upgrade.sh,v 1.2.4.5 1996/08/27 18:15:08 gwr Exp $
#
# Copyright (c) 1997-2002 Todd Miller, Theo de Raadt, Ken Westerback
# All rights reserved.
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

#	OpenBSD installation script.
#	In a perfect world, this would be a nice C program, with a reasonable
#	user interface.

# install.sub needs to know the MODE
MODE=upgrade

# include common subroutines and initialization code
. install.sub

# Remove 'etc' set from THESETS. It should be installed
# manually, after the upgrade. Note that etc should not
# be the first or last set in THESETS, or this won't
# work!
THESETS=`echo $THESETS | sed -e 's/ etc / /'`

# XXX Work around vnode aliasing bug (thanks for the tip, Chris...)
ls -l /dev > /dev/null 2>&1

while [ -z "$ROOTDISK" ]; do
	getrootdisk
done

# Assume partition 'a' of $ROOTDISK is for the root filesystem. Confirm
# this with the user. Check and mount the root filesystem.
resp=
while [ -z "$resp" ]; do
	ask "Root filesystem?" "${ROOTDISK}a"
	_root_filesystem=/dev/`basename $resp`
	if [ ! -b ${_root_filesystem} ]; then
		echo "Sorry, ${_root_filesystem} is not a block device."
		resp=
	fi
done

echo -n "Checking root filesystem (fsck -fp ${_root_filesystem}) ... "
if fsck -fp ${_root_filesystem} > /dev/null 2>&1; then
	echo	"OK."
else
	echo	"FAILED.\nYou must fsck ${_root_filesystem} manually."
	exit 1
fi

echo -n "Mounting root filesystem ... "
if mount -o ro ${_root_filesystem} /mnt; then
	echo	"Done."
else
	echo	"ERROR: can't mount root filesystem!"
	exit 1
fi

# fstab and hosts are required for upgrade
for _file in fstab hosts; do
	if [ ! -f /mnt/etc/$_file ]; then
		echo "ERROR: no /etc/${_file}!"
		exit 1
	fi
	cp /mnt/etc/$_file /tmp/$_file
done

# Start up the network in same/similar configuration as the installed system
# uses.
cat << __EOT

The upgrade program would now like to enable the network. It will use the
configuration already stored on the root filesystem. This is required
if you wish to use the network installation capabilities of this program.

__EOT
ask "Enable network?" y
case $resp in
y*|Y*)
	if ! enable_network; then
		echo "ERROR: can't enable network!"
		exit 1
	fi

	cat << __EOT

You will now be given the opportunity to escape to the command shell to
do any additional network configuration you may need. This may include
adding additional routes, if needed. In addition, you might take this
opportunity to redo the default route in the event that it failed above.

__EOT
	ask "Escape to shell?" n
	case $resp in
	y*|Y*)	echo "Type 'exit' to return to upgrade."
		sh
		;;
	esac
	;;
esac

echo	"The fstab is configured as follows:\n"
cat /tmp/fstab

cat << __EOT

You may wish to edit the fstab. For example, you may need to resolve
dependencies in the order which the filesystems are mounted.

NOTE:	1) this fstab is used only during the upgrade. It will not be
	   copied into the root filesystem.

	2) all non-ffs filesystems, and filesystems with the 'noauto'
	   option, will be ignored during the upgrade.

__EOT
ask "Edit the fstab with ${EDITOR}?" n
case $resp in
y*|Y*)	${EDITOR} /tmp/fstab
	;;
esac

echo

# Create a fstab containing only ffs filesystems w/o 'noauto'.
munge_fstab < /tmp/fstab

if ! umount /mnt; then
	echo	"ERROR: can't unmount previously mounted root!"
	exit 1
fi

# Check filesystems.
echo "Checking non-root filesystems..."
if ! check_fs $_root_filesystem; then
	# Prevent check_fs() invocation in cleanup_on_exit from fsck'ing.
	# Remember /etc/fstab is a link, /tmp/fstab.shadow is the file!
	rm /tmp/fstab.shadow
fi
echo "...Done."

if [ ! -f /etc/fstab ]; then
	exit 2
else
	# Mount filesystems.
	mount_fs
fi

# If Xfree86 v3 directories that would prevent upgrading to XFree86 v4
# are found, move them and replace them with links that the upgrade
# can replace with new values.
(
if [ -d /mnt/usr/X11R6/lib/X11 ]; then
	cd /mnt/usr/X11R6/lib/X11
	for xf3dir in twm xkb xsm xinit rstart; do
		if [ -e $xf3dir -a ! -L $xf3dir ]; then
			mkdir -p XF3
			mv $xf3dir XF3/.
			ln -s XF3/$xf3dir $xf3dir
		fi
	done
fi
)

ask "Are the upgrade sets on one of your normally mounted (local) filesystems?" y
case $resp in
y*|Y*)	get_localdir /mnt
	;;
esac

# Install sets.
install_sets $THESETS

# Copy in configuration information and make devices in target root.
(
	if [ -f /mnt/etc/sendmail.cf -a ! -f /mnt/etc/mail/sendmail.cf ]; then
		echo "Moving /etc/sendmail.cf -> /etc/mail/sendmail.cf"
		[ -d /mnt/etc/mail ] || mkdir /mnt/etc/mail
		mv /mnt/etc/sendmail.cf /mnt/etc/mail/sendmail.cf
		ed - /mnt/etc/rc << __EOT
1,$s/etc\/sendmail.cf/etc\/mail\/sendmail.cf/g
w
q
__EOT
	fi
)

# Perform final steps common to both an install and an upgrade.
finish_up
