#	$OpenBSD: install.md,v 1.8 1997/05/01 11:30:43 niklas Exp $
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
MDSETS=""
# TTT MDSETS="xbin xman xinc xcon"

md_set_term() {
	if [ ! -z "$TERM" ]; then
		return
	fi
	echo -n "Specify terminal type [vt100]: "
	getresp "vt100"
	TERM="$resp"
	export TERM
}

md_makerootwritable() {
	# Was: do_mfs_mount "/tmp" "2048"
	# /tmp is the mount point
	# 2048 is the size in DEV_BIZE blocks

# TTT	umount /tmp > /dev/null 2>&1
#	if ! mount_mfs -s 2048 swap /tmp ; then
#		cat << \__mfs_failed_1
#
#FATAL ERROR: Can't mount the memory filesystem.
#
#__mfs_failed_1
#		exit
#	fi

	# Bleh.  Give mount_mfs a chance to DTRT.
#	sleep 2
	
	md_mountkernfs
}

md_mountkernfs() {
        if [ -e ${KERNFSMOUNTED} ]
        then
                return
        fi
	if [ ! -d /kern ]; then
		mkdir /kern
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

md_get_diskdevs() {
	# return available disk devices
	grep "^rz[0-6] " < /kern/msgbuf | cut -d" " -f1 | sort -u
}

md_get_cddevs() {
	# return available CDROM devices
	grep "^rz[0-6] " < /kern/msgbuf | cut -d" " -f1 | sort -u
}

md_get_ifdevs() {
	# return available network devices
	grep "^le[0-9] " < /kern/msgbuf | cut -d" " -f1 | sort -u
}

md_get_partition_range() {
    # return range of valid partition letters
    echo "[a-h]"
}


md_installboot() {
        echo -n "Installing boot block..."
        # $1 is the root disk
        disklabel -W ${1}
        disklabel -B ${1}
        echo "done."
	# we also use this chance to do an ldconfig here
	echo -n "creating runtime link editor directory cache..."
	chroot /mnt ldconfig
	echo "done."
}

md_native_fstype() {
}

md_native_fsopts() {
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
	0)
		echo -n "Do you wish to edit the disklabel on $_disk? [y]"
		;;
	1)
		echo "WARNING: Disk $_disk has no label"
		echo -n "Do you want to create one with the disklabel editor? [y]"
		;;
	2)
		echo "WARNING: Label on disk $_disk is corrupted"
		echo -n "Do you want to try and repair the damage using the disklabel editor? [y]"
		;;
	esac

	getresp "y"
	case "$resp" in
	y*|Y*) ;;
	*)	return ;;
	esac

	# display example
	cat << \__md_prep_disklabel_1

Here is an example of what the partition information will look like once
you have entered the disklabel editor. Disk partition sizes and offsets
are in sector (most likely 512 bytes) units.

Make sure these size/offset pairs are on cylinder boundaries (the number
of sector per cylinder is given in the `sectors/cylinder' entry.

If this disk is previously un-labeled, only the "c" partition will show up
in the editor and you will have to enter lines similar to those shown in the
example for the other paritions.  If you are uncertain about the syntax or
space requirements, this is a good time to review the installation notes.

Do not change any parameters except the partition layout and the label name.
It's probably also wisest not to touch the `8 partitions:' line, even
in case you have defined less than sixteen partitions.

[** EXAMPLE **]
# /dev/rrz0c:
type: SCSI
disk: SEAGATE ST1480 r
label: 
flags:
bytes/sector: 512
sectors/track: 63
tracks/cylinder: 11
sectors/cylinder: 693
cylinders: 832527
total sectors: 832527
rpm: 3600
interleave: 1
trackskew: 0
cylinderskew: 0
headswitch: 0           # milliseconds
track-to-track seek: 0  # milliseconds
drivedata: 0 

8 partitions:
#        size   offset    fstype   [fsize bsize   cpg]
  a:    65536        0    4.2BSD     1024  8192    16   # (Cyl.   0 - 94*)
  b:   131072    65536      swap                        # (Cyl.  94*- 283*)
  c:   832527        0    unused     1024  8192         # (Cyl.   0 - 1201*)
  d:   635919   196608    4.2BSD     1024  8192    16   # (Cyl. 283*- 1201*)
[End of **EXAMPLE**]

__md_prep_disklabel_1
	echo -n "Press [Enter] to continue "
	getresp ""
	disklabel -W ${_disk}
# TTT   hack to workaround disklabel problems
	disklabel ${_disk} > /tmp/tempdisklabel
	disklabel -r -R ${_disk} /tmp/tempdisklabel 2> /dev/null
	disklabel -e ${_disk}
# TTT   hack to workaround disklabel problems
	disklabel ${_disk} > /tmp/tempdisklabel
	disklabel -r -R ${_disk} /tmp/tempdisklabel 2> /dev/null
}

md_copy_kernel() {
	echo -n "Copying kernel..."
	cp -p /bsd /mnt/bsd
	echo "done."
}

md_welcome_banner() {
{
	if [ "$MODE" = "install" ]; then
		echo ""
		echo "Welcome to the OpenBSD/pmax ${VERSION} installation program."
		cat << \__welcome_banner_1

This program is designed to help you put OpenBSD on your disk,
in a simple and rational way.  You'll be asked several questions,
and it would probably be useful to have your disk's hardware
manual, the installation notes, and a calculator handy.
__welcome_banner_1

	else
		echo ""
		echo "Welcome to the OpenBSD/pmax ${VERSION} upgrade program."
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

As with anything which modifies your disk's contents, this
program can cause SIGNIFICANT data loss, and you are advised
to make sure your data is backed up before beginning the
installation process.

Default answers are displayed in brackets after the questions.
You can hit Control-C at any time to quit, but if you do so at a
prompt, you may have to hit return.  Also, quitting in the middle of
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
