#!/bin/sh
#	$OpenBSD: install.sh,v 1.107 2002/07/13 16:32:13 krw Exp $
#	$NetBSD: install.sh,v 1.5.2.8 1996/08/27 18:15:05 gwr Exp $
#
# Copyright (c) 1997-2002 Todd Miller, Theo de Raadt, Ken Westerback
# All rights reserved.
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
#	This product includes software developed by Todd Miller and
#	Theo de Raadt
# 4. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

# A list of devices holding filesystems and the associated mount points
# is kept in the file named FILESYSTEMS.
FILESYSTEMS=/tmp/filesystems

# The Fully Qualified Domain Name
FQDN=

# install.sub needs to know the MODE
MODE=install

# include common subroutines and initialization code
. install.sub

# If /etc/fstab already exists we skip disk initialization, but we still
# need to know the root disk.
if [ -f /etc/fstab ]; then
	get_rootdisk
else
	# Install the shadowed disktab file; lets us write to it for temporary
	# purposes without mounting the miniroot read-write.
	[ -f /etc/disktab.shadow ] && cp /etc/disktab.shadow /tmp/disktab.shadow

	while : ; do
		if [ -z "$ROOTDISK" ]; then
			# Get ROOTDISK and default ROOTDEV
			get_rootdisk
			DISK=$ROOTDISK
			echo "${ROOTDEV} /" > $FILESYSTEMS
		else
			cat << __EOT

Now you can select another disk to initialize. (Do not re-select a disk
you have already entered information for).
__EOT
			ask_fordev "Which disk do you wish to initialize?" "$DKDEVS"
			[ "$resp" = "done" ] && break
			DISK=$resp
		fi

		# Deal with disklabels, including editing the root disklabel
		# and labeling additional disks. This is machine-dependent since
		# some platforms may not be able to provide this functionality.
		md_prep_disklabel ${DISK}

		# Assume $ROOTDEV is the root filesystem, but loop to get the rest.
		# XXX ASSUMES THAT THE USER DOESN'T PROVIDE BOGUS INPUT.
		cat << __EOT

You will now have the opportunity to enter filesystem information for ${DISK}.
You will be prompted for the mount point (full path, including the prepending
'/' character) for each BSD partition on ${DISK}. Enter "none" to skip a
partition or "done" when you are finished.
__EOT

		if [ "$DISK" = "$ROOTDISK" ]; then
			cat << __EOT

The following partitions will be used for the root filesystem and swap:
	${ROOTDEV}	/
	${ROOTDISK}b	swap

__EOT
		fi

		# XXX - allow the user to name mount points on disks other than ROOTDISK
		#	also allow a way to enter non-BSD partitions (but don't newfs!)
		# Get the list of BSD partitions and store sizes
		_npartitions=0

		# XXX - It would be nice to just pipe the output of sed to a
		#       'while read _pp _ps' loop, but our 'sh' runs the last
		#       element of a pipeline in a subshell and the required side
		#       effects to _partitions, _npartitions, etc. would be lost.
		for _p in $(disklabel ${DISK} 2>&1 | sed -ne '/^ *\([a-p]\): *\([0-9][0-9]*\).*BSD.*/s//\1\2/p'); do
			# All characters after the initial [a-p] are the partition size
			_ps=${_p#?}
			# Removing the partition size leaves us with the partition name
			_pp=${_p%${_ps}}

			[ "${DISK}${_pp}" = "$ROOTDEV" ] && continue

			_partitions[$_npartitions]=$_pp
			_psizes[$_npartitions]=$_ps
			# If the user assigned a mount point, use it.
			if [ -f /tmp/fstab.$DISK ]; then
				_mount_points[$_npartitions]=`sed -n "s:^/dev/${DISK}${_pp}[ 	]*\([^ 	]*\).*:\1:p" < /tmp/fstab.${DISK}`
			fi
			: $(( _npartitions += 1 ))
		done

		# Now prompt the user for the mount points. Loop until "done"
		echo
		_i=0
		resp=
		while [ $_npartitions -gt 0 -a "$resp" != "done" ]; do
			_pp=${_partitions[${_i}]}
			_ps=$(( ${_psizes[${_i}]} / 2 ))
			_mp=${_mount_points[${_i}]}

			# Get the mount point from the user
			while : ; do
				ask "Mount point for ${DISK}${_pp} (size=${_ps}k), RET, none or done?" "$_mp"
				case $resp in
				/*)	_mount_points[${_i}]=$resp
					break
					;;
				done|"")break
					;;
				none)	_mount_points[${_i}]=
					break
					;;
				*)	echo "mount point must be an absolute path!"
					break
					;;
				esac
			done
			_i=$(( $_i + 1 ))
			[ $_i -ge $_npartitions ] && _i=0
		done

		# Now write it out, sorted by mount point
		for _mp in `bsort ${_mount_points[*]}`; do
			_i=0
			while [ $_i -lt $_npartitions ] ; do
				if [ $_mp = "${_mount_points[${_i}]}" ]; then
					echo "${DISK}${_partitions[${_i}]} ${_mount_points[${_i}]}" >> ${FILESYSTEMS}
					_mount_points[${_i}]=
					break
				fi
				_i=$(( ${_i} + 1 ))
			done
		done
		rm -f /tmp/fstab.${DISK}
	done

	cat << __EOT

You have configured the following devices and mount points:

$(<${FILESYSTEMS})

============================================================
The next step will overwrite any existing data on:
__EOT
	(
		echo -n "	"
		while read _device_name _junk; do
			echo -n "$_device_name "
		done
		echo
	) < ${FILESYSTEMS}

	ask "\nAre you really sure that you're ready to proceed?" n
	case $resp in
	y*|Y*)	;;
	*)	echo "ok, try again later..."
		exit
		;;
	esac

	# Loop though the file, place filesystems on each device.
	echo	"Creating filesystems..."
	(
		while read _device_name _junk; do
			newfs -q /dev/r${_device_name}
		done
	) < ${FILESYSTEMS}
fi

# Get network configuration information, and store it for placement in the
# root filesystem later.
cat << __EOT

You will now be given the opportunity to configure the network. This will be
useful if you need to transfer the installation sets via FTP, HTTP, or NFS.
Even if you choose not to transfer installation sets that way, this information
will be preserved and copied into the new root filesystem.

__EOT
ask "Configure the network?" y
case $resp in
y*|Y*)	donetconfig
	;;
esac

if [ ! -f /etc/fstab ]; then
	# Now that the network has been configured, it is safe to configure the
	# fstab.
	(
		while read _dev _mp; do
			case $_mp in
			"/")	echo /dev/$_dev $_mp ffs rw 1 1;;
			"/tmp"|"/var/tmp") echo /dev/$_dev $_mp ffs rw,nosuid,nodev 1 2;;
			*)	echo /dev/$_dev $_mp ffs rw 1 2;;
			esac
		done
	) < ${FILESYSTEMS} > /tmp/fstab

	munge_fstab
fi

mount_fs "-o async"

echo '\nPlease enter the initial password that the root account will have.'
_oifs=$IFS
IFS=
resp=
while [ -z "$resp" ]; do
	askpass "Password (will not echo):"
	_password=$resp

	askpass "Password (again):"
	if [ "$_password" != "$resp" ]; then
		echo "Passwords do not match, try again."
		resp=
	fi
done
IFS=$_oifs

install_sets $THESETS

# Set machdep.apertureallowed if required. install_sets must be
# done first so that /etc/sysctl.conf is available.
set_machdep_apertureallowed
	
# Copy configuration files to /mnt/etc.
cfgfiles="fstab hostname.* hosts myname mygate resolv.conf kbdtype sysctl.conf"

echo
if [ -f /etc/dhclient.conf ]; then
	echo -n "Saving dhclient configuration..."
	cat /etc/dhclient.conf >> /mnt/etc/dhclient.conf
	echo "lookup file bind" > /mnt/etc/resolv.conf.tail
	cp /var/db/dhclient.leases /mnt/var/db/.
	# Don't install mygate for dhcp installations.
	# Note that mygate should not be the first or last file
	# in cfgfiles or this won't work.
	cfgfiles=`echo $cfgfiles | sed -e 's/ mygate / /'`
	echo "done."
fi

cd /tmp
echo -n "Copying... "
for file in $cfgfiles; do
	if [ -f $file ]; then
		echo -n "$file "
		cp $file /mnt/etc/$file
		rm -f $file
	fi
done
echo "...done."

remount_fs

_encr=`/mnt/usr/bin/encrypt -b 7 -- "$_password"`
echo "1,s@^root::@root:${_encr}:@
w
q" | ed /mnt/etc/master.passwd 2> /dev/null
/mnt/usr/sbin/pwd_mkdb -p -d /mnt/etc /etc/master.passwd

echo -n "Generating initial host.random file ..."
dd if=/mnt/dev/urandom of=/mnt/var/db/host.random bs=1024 count=64 >/dev/null 2>&1
chmod 600 /mnt/var/db/host.random >/dev/null 2>&1
echo "...done."

# Perform final steps common to both an install and an upgrade.
finish_up
