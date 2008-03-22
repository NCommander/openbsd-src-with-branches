#	$OpenBSD: install.md,v 1.7 2008/03/04 00:36:38 krw Exp $
#
# machine dependent section of installation/upgrade script.
#

MDTERM=vt100
ARCH=ARCH

md_installboot() {
	echo -n "Installing boot block..."
	/sbin/disklabel -B $1
	echo "done."
}

md_prep_disklabel() {
	local _disk=$1

	disklabel -W $_disk >/dev/null 2>&1
	disklabel -f /tmp/fstab.$_disk -E $_disk
}

md_congrats() {
}

md_consoleinfo() {
}
