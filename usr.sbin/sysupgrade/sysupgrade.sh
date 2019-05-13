#!/bin/ksh
#
# $OpenBSD: sysupgrade.sh,v 1.19 2019/05/10 01:29:14 naddy Exp $
#
# Copyright (c) 1997-2015 Todd Miller, Theo de Raadt, Ken Westerback
# Copyright (c) 2015 Robert Peichaer <rpe@openbsd.org>
# Copyright (c) 2016, 2017 Antoine Jacoutot <ajacoutot@openbsd.org>
# Copyright (c) 2019 Christian Weisgerber <naddy@openbsd.org>
# Copyright (c) 2019 Florian Obser <florian@openbsd.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

set -e
umask 0022

ARCH=$(uname -m)

ug_err()
{
	echo "${1}" 1>&2 && return ${2:-1}
}

usage()
{
	ug_err "usage: ${0##*/} [-fkn] [-r | -s] [installurl]"
}

unpriv()
{
	local _file _rc=0 _user=_syspatch

	if [[ $1 == -f ]]; then
		_file=$2
		shift 2
	fi
 	if [[ -n ${_file} ]]; then
		>${_file}
		chown "${_user}" "${_file}"
	fi
	(($# >= 1))

	eval su -s /bin/sh ${_user} -c "'$@'" || _rc=$?

	[[ -n ${_file} ]] && chown root "${_file}"

	return ${_rc}
}

# df(1) is run twice to sort our prefered FS list first
get_prefetcharea() {
	{ df -kl /{tmp,home,usr{/local,}}; df -kl ;} |
		while read a a a a m m; do
			[[ $m == /@(tmp|home|usr/@(src,obj,xobj))@(|/*) ]] &&
				((a > 524288)) && echo $m && break
			[[ $m == @(|/*) ]] &&
				((a > 524288 * 4)) && echo $m && break
		done
}

# Remove all occurrences of first argument from list formed by the remaining
# arguments.
rmel() {
	local _a=$1 _b _c

	shift
	for _b; do
		[[ $_a != "$_b" ]] && _c="${_c:+$_c }$_b"
	done
	echo -n "$_c"
}

RELEASE=false
SNAP=false
FORCE=false
KEEP=false
REBOOT=true

while getopts fknrs arg; do
	case ${arg} in
	f)	FORCE=true;;
	k)	KEEP=true;;
	n)	REBOOT=false;;
	r)	RELEASE=true;;
	s)	SNAP=true;;
	*)	usage;;
	esac
done

(($(id -u) != 0)) && ug_err "${0##*/}: need root privileges"

if $RELEASE && $SNAP; then
	usage
fi

set -A _KERNV -- $(sysctl -n kern.version |
	sed 's/^OpenBSD \([0-9]\)\.\([0-9]\)\([^ ]*\).*/\1.\2 \3/;q')

shift $(( OPTIND -1 ))

case $# in
0)	MIRROR=$(sed 's/#.*//;/^$/d' /etc/installurl) 2>/dev/null ||
		MIRROR=https://cdn.openbsd.org/pub/OpenBSD
	;;
1)	MIRROR=$1
	;;
*)	usage
esac

if ! $RELEASE && [[ ${#_KERNV[*]} == 2 ]]; then
	SNAP=true
fi

NEXT_VERSION=$(echo ${_KERNV[0]} + 0.1 | bc)

if $SNAP; then
	URL=${MIRROR}/snapshots/${ARCH}/
else
	URL=${MIRROR}/${NEXT_VERSION}/${ARCH}/
fi

SETSDIR=$(mktemp -d -p $(get_prefetcharea) .sysupgrade.XXXXXXXXXX)

if [[ -n ${SETSDIR} ]]; then
	install -d -o 0 -g 0 -m 0755 ${SETSDIR}
	cat <<-__EOT >/auto_upgrade.conf
		Location of sets = disk
		Pathname to the sets = ${SETSDIR}
		Set name(s) = done
		Directory does not contain SHA256.sig. Continue without verification = yes
	__EOT
	${KEEP} || echo "rm -rf -- \"${SETSDIR}\"" >>/etc/rc.firsttime

else
	ug_err "Not enough space to fetch sets"
fi

cd ${SETSDIR}

unpriv -f SHA256.sig ftp -Vmo SHA256.sig ${URL}SHA256.sig

_KEY=openbsd-${_KERNV[0]%.*}${_KERNV[0]#*.}-base.pub
_NEXTKEY=openbsd-${NEXT_VERSION%.*}${NEXT_VERSION#*.}-base.pub

read _LINE <SHA256.sig
case ${_LINE} in
*\ ${_KEY})	SIGNIFY_KEY=/etc/signify/${_KEY} ;;
*\ ${_NEXTKEY})	SIGNIFY_KEY=/etc/signify/${_NEXTKEY} ;;
*)		ug_err "invalid signing key" ;;
esac

[[ -f ${SIGNIFY_KEY} ]] || ug_err "cannot find ${SIGNIFY_KEY}"

unpriv -f SHA256 signify -Ve -p "${SIGNIFY_KEY}" -x SHA256.sig -m SHA256
rm SHA256.sig

if cmp -s /var/db/installed.SHA256 SHA256 && ! $FORCE; then
	echo "Already on latest snapshot."
	exit 0
fi

# INSTALL.*, bsd*, *.tgz
SETS=$(sed -n -e 's/^SHA256 (\(.*\)) .*/\1/' \
    -e '/^INSTALL\./p;/^bsd/p;/\.tgz$/p' SHA256)

OLD_FILES=$(ls)
OLD_FILES=$(rmel SHA256 $OLD_FILES)
DL=$SETS

[[ -n ${OLD_FILES} ]] && echo Verifying old sets.
for f in ${OLD_FILES}; do
	if cksum -C SHA256 $f >/dev/null 2>&1; then
		DL=$(rmel $f ${DL})
		OLD_FILES=$(rmel $f ${OLD_FILES})
	fi
done

[[ -n ${OLD_FILES} ]] && rm ${OLD_FILES}
for f in ${DL}; do
	unpriv -f $f ftp -Vmo ${f} ${URL}${f}
done

if [[ -n ${DL} ]]; then
	echo Verifying sets.
	unpriv cksum -qC SHA256 ${DL}
fi

cp bsd.rd /nbsd.upgrade
ln -f /nbsd.upgrade /bsd.upgrade
rm /nbsd.upgrade

if ${REBOOT}; then
	echo Upgrading.
	exec reboot
else
	echo "Will upgrade on next reboot"
fi
