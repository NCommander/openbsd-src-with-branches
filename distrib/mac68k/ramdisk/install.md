#       $OpenBSD: install.md,v 1.9 2002/04/28 14:44:01 krw Exp $
#
# Copyright (c) 2002, Miodrag Vallat.
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
#
# THIS SOFTWARE IS PROVIDED BY ITS AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO
# EVENT SHALL ITS AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
#

# Machine-dependent install sets
MDSETS="bsd-generic bsd-genericsbc"
MDTERM=vt100
ARCH=ARCH

md_set_term() {
}

md_get_diskdevs() {
	# return available disk devices
	bsort `dmesg | egrep -a "^[sw]d[0-9]+ " | cutword 1`
}

md_get_cddevs() {
	# return available CDROM devices
	bsort `dmesg | egrep -a "^cd[0-9]+ " | cutword 1`
}

md_questions() {
	:
}

md_installboot() {
	# no standalone boot block
	:
}

md_native_fstype() {
	:
}

md_native_fsopts() {
	:
}

md_checkfordisklabel() {
	# $1 is the disk to check
	local rval

	disklabel $1 > /dev/null 2> /tmp/checkfordisklabel
	if grep "no OpenBSD or MacOS disk label" /tmp/checkfordisklabel; then
		rval=1
	elif grep "disk label corrupted" /tmp/checkfordisklabel; then
		rval=2
	elif grep " HFS " /tmp/checkfordisklabel; then
		rval=3
	else
		rval=0
	fi

	rm -f /tmp/checkfordisklabel
	return $rval
}

md_prep_disklabel() {
	local _disk=$1
	local _wflag="-W"

	md_checkfordisklabel $_disk
	case $? in
	0)	;;
	1)	echo WARNING: Label on disk $_disk has no label. You will be creating a new one.
		echo
	;;
	2)	echo WARNING: Label on disk $_disk is corrupted. You will be repairing.
		echo
	;;
	3)	echo WARNING: This disk has been set up under Mac OS. For safety reasons, you
		echo will not be allowed to save any disklabel changes from OpenBSD.
		echo
		_wflag="-N"
	;;
	esac

	# display example
	cat << __EOT
If you are unsure of how to use multiple partitions properly
(ie. separating /, /usr, /tmp, /var, /usr/local, and other things)
just split the space into a root and swap partition for now.

__EOT
	disklabel ${_wflag} ${_disk}
	disklabel -f /tmp/fstab.${_disk} -E ${_disk}
}

md_welcome_banner() {
{
	if [ "$MODE" = "install" ]; then
		cat << __EOT
Welcome to the OpenBSD/mac68k ${VERSION_MAJOR}.${VERSION_MINOR} installation program.

This program is designed to help you put OpenBSD on your disk, in a simple and
rational way.
__EOT

	else
		cat << __EOT
Welcome to the OpenBSD/mac68k ${VERSION_MAJOR}.${VERSION_MINOR} upgrade program.

This program is designed to help you upgrade your OpenBSD system in a simple
and rational way.  As a reminder, installing the 'etc' binary set is NOT
recommended.  Once the rest of your system has been upgraded, you should
manually merge any changes to files in the 'etc' set into those files which
already exist on your system.
__EOT
	fi

cat << __EOT

As with anything which modifies your disk's contents, this
program can cause SIGNIFICANT data loss, and you are advised
to make sure your data is backed up before beginning the
installation process.

Default answers are displayed in brackets after the questions.
You can hit Control-C at any time to quit, but if you do so at a
prompt, you may have to hit return.  Also, quitting in the middle of
installation may leave your system in an inconsistent state.

__EOT
} | more
}

md_not_going_to_install() {
	cat << __EOT

OK, then.  Enter `halt' at the prompt to halt the machine.  Once the
machine has halted, power-cycle the system to load new boot code.

__EOT
}

md_congrats() {
	local what;
	if [ "$MODE" = "install" ]; then
		what="installed";
	else
		what="upgraded";
	fi
	cat << __EOT

CONGRATULATIONS!  You have successfully $what OpenBSD!  To boot the
installed system, enter halt at the command prompt. Once the system has
halted, reset the machine and boot from the disk.

__EOT
}
