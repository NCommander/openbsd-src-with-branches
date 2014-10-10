#!/bin/sh
#
# $OpenBSD: rcctl.sh,v 1.40 2014/10/10 15:42:30 ajacoutot Exp $
#
# Copyright (c) 2014 Antoine Jacoutot <ajacoutot@openbsd.org>
# Copyright (c) 2014 Ingo Schwarze <schwarze@openbsd.org>
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

_special_services="accounting check_quotas ipsec multicast_host multicast_router pf spamd_black"
readonly _special_services

# get local functions from rc.subr(8)
FUNCS_ONLY=1
. /etc/rc.d/rc.subr
_rc_parse_conf

usage()
{
	_rc_err "usage: ${0##*/} [-df] enable|disable|status|action
             [service | daemon [flags [arguments]]]"
}

needs_root()
{
	[ "$(id -u)" -ne 0 ] && _rc_err "${0##*/} $1: need root privileges"
}

rcconf_edit_begin()
{
	_TMP_RCCONF=$(mktemp -p /etc -t rc.conf.local.XXXXXXXXXX) || exit 1
	if [ -f /etc/rc.conf.local ]; then
		# only to keep permissions (file content is not needed)
		cp -p /etc/rc.conf.local ${_TMP_RCCONF} || exit 1
	else
		touch /etc/rc.conf.local || exit 1
	fi
}

rcconf_edit_end()
{
	sort -u -o ${_TMP_RCCONF} ${_TMP_RCCONF} || exit 1
	mv ${_TMP_RCCONF} /etc/rc.conf.local || exit 1
	if [ ! -s /etc/rc.conf.local ]; then
		rm /etc/rc.conf.local || exit 1
	fi
}

svc_default_enabled()
{
	local _svc=$1
	[ -n "${_svc}" ] || return
	local _ret=1

	_rc_parse_conf /etc/rc.conf
	svc_is_enabled ${_svc} && _ret=0
	_rc_parse_conf

	return ${_ret}
}

# for security reason and to prevent namespace pollution, only call in a
# subshell against base system daemons or disabled package scripts
svc_default_enabled_flags()
{
	local _svc=$1
	[ -n "${_svc}" ] || return

	FUNCS_ONLY=1
	rc_cmd() { }
	. /etc/rc.d/${_svc} >/dev/null 2>&1
	[ -n "${daemon_flags}" ] && print -r -- ${daemon_flags}
}

svc_get_flags()
{
	local _svc=$1
	[ -n "${_svc}" ] || return
	local daemon_flags

	if svc_is_special ${_svc}; then
		echo "$(eval echo \${${_svc}})"
	else
		# set pkg daemon_flags to "NO" to match base svc
		if ! svc_is_base ${_svc}; then
			if ! echo ${pkg_scripts} | grep -qw ${_svc}; then
				echo "NO" && return
			fi
		fi
		[ -z "${daemon_flags}" ] && \
			daemon_flags="$(eval echo \${${_svc}_flags})"
		[ -z "${daemon_flags}" ] && \
			daemon_flags="$(svc_default_enabled_flags ${_svc})"

		print -r -- ${daemon_flags} | sed '/^$/d'
	fi
}

svc_get_status()
{
	local _i _svc=$1

	if [ -n "${_svc}" ]; then
		svc_get_flags ${_svc}
		svc_is_enabled ${_svc}
	else
		for _i in $(ls -A /etc/rc.d | grep -v rc.subr); do
			echo "${_i}_flags=$(svc_get_flags ${_i})"
		done
		for _i in ${_special_services}; do
			echo "${_i}=$(svc_get_flags ${_i})"
		done
	fi
}

svc_is_avail()
{
	local _svc=$1
	[ -n "${_svc}" ] || return 1

	[ "${_svc}" = "rc.subr" ] && return 1
	[ -x "/etc/rc.d/${_svc}" ] && return 0
	svc_is_special ${_svc}
}

svc_is_base()
{
	local _svc=$1
	[ -n "${_svc}" ] || return

	grep "^start_daemon " /etc/rc | cut -d ' ' -f2- | grep -qw ${_svc}
}

svc_is_enabled()
{
	local _svc=$1
	[ -n "${_svc}" ] || return

	[ "$(svc_get_flags ${_svc})" != "NO" ]
}

svc_is_special()
{
	local _svc=$1
	[ -n "${_svc}" ] || return

	echo ${_special_services} | grep -qw ${_svc}
}

append_to_pkg_scripts()
{
	local _svc=$1
	[ -n "${_svc}" ] || return

	rcconf_edit_begin
	if [ -z "${pkg_scripts}" ]; then
		echo pkg_scripts="${_svc}" >>${_TMP_RCCONF}
	elif ! echo ${pkg_scripts} | grep -qw ${_svc}; then
		grep -v "^pkg_scripts.*=" /etc/rc.conf.local >${_TMP_RCCONF}
		echo pkg_scripts="${pkg_scripts} ${_svc}" >>${_TMP_RCCONF}
	fi
	rcconf_edit_end
}

rm_from_pkg_scripts()
{
	local _svc=$1
	[ -n "${_svc}" ] || return

	[ -z "${pkg_scripts}" ] && return

	rcconf_edit_begin
	sed "/^pkg_scripts[[:>:]]/{s/[[:<:]]${_svc}[[:>:]]//g
	    s/['\"]//g;s/ *= */=/;s/   */ /g;s/ $//;/=$/d;}" \
	    /etc/rc.conf.local >${_TMP_RCCONF}
	rcconf_edit_end
}

add_flags()
{
	local _svc=$2
	[ -n "${_svc}" ] || return

	if svc_is_special ${_svc}; then
		rcconf_edit_begin
		grep -v "^${_svc}.*=" /etc/rc.conf.local >${_TMP_RCCONF}
		if ! svc_default_enabled ${_svc}; then
			echo "${_svc}=YES" >>${_TMP_RCCONF}
		fi
		rcconf_edit_end
		return
	fi

	local _flags

	if [ -n "$3" ]; then
		shift 3
		_flags=$*
	else
		# keep our flags since none were given
		eval "_flags=\${${_svc}_flags}"
		[ "${_flags}" = "NO" ] && unset _flags
	fi

	# unset flags if they match the default enabled ones
	if [ -n "${_flags}" ]; then
		[ "${_flags}" = "$(svc_default_enabled_flags ${_svc})" ] && \
			unset _flags
	fi

	rcconf_edit_begin
	grep -v "^${_svc}_flags.*=" /etc/rc.conf.local >${_TMP_RCCONF}
	if [ -n "${_flags}" ] || \
	   ( svc_is_base ${_svc} && ! svc_default_enabled ${_svc} ); then
		echo ${_svc}_flags=${_flags} >>${_TMP_RCCONF}
	fi
	rcconf_edit_end
}

rm_flags()
{
	local _svc=$1
	[ -n "${_svc}" ] || return

	rcconf_edit_begin
	if svc_is_special ${_svc}; then
		grep -v "^${_svc}.*=" /etc/rc.conf.local >${_TMP_RCCONF}
		if svc_default_enabled ${_svc}; then
			echo "${_svc}=NO" >>${_TMP_RCCONF}
		fi
	else
		grep -v "^${_svc}_flags.*=" /etc/rc.conf.local >${_TMP_RCCONF}
		if svc_default_enabled ${_svc}; then
			echo "${_svc}_flags=NO" >>${_TMP_RCCONF}
		fi
	fi
	rcconf_edit_end
}

unset _RC_DEBUG _RC_FORCE
while getopts "df" c; do
	case "$c" in
		d) _RC_DEBUG=-d;;
		f) _RC_FORCE=-f;;
		*) usage;;
	esac
done
shift $((OPTIND-1))
[ $# -gt 0 ] || usage

action=$1
svc=$2
flag=$3
flags=$*

if [ -n "$svc" ]; then
	if ! svc_is_avail $svc; then
		_rc_err "${0##*/}: service $svc does not exist" 2
	fi
elif [ "$action" != "status" ]; then
	usage
fi

if [ -n "$flag" ]; then
	if [ "$flag" = "flags" ]; then
		if [ "$action" != "enable" ]; then
			_rc_err "${0##*/}: \"flags\" can only be set with \"enable\""
		fi
		if svc_is_special $svc && [ -n "$4" ]; then
			_rc_err "${0##*/}: \"$svc\" is a special variable, cannot set \"flags\""
		fi
		if [ "$4" = "NO" ]; then
			_rc_err "${0##*/}: \"flags NO\" contradicts \"enable\""
		fi
	else
		usage
	fi
fi

case $action in
	disable)
		needs_root $action
		if ! svc_is_base $svc && ! svc_is_special $svc; then
			rm_from_pkg_scripts $svc
		fi
		rm_flags $svc
		;;
	enable)
		needs_root $action
		add_flags $flags
		if ! svc_is_base $svc && ! svc_is_special $svc; then
			append_to_pkg_scripts $svc
		fi
		;;
	status)
		svc_get_status $svc
		;;
	start|stop|restart|reload|check)
		if svc_is_special $svc; then
			_rc_err "${0##*/}: \"$svc\" is a special variable, no rc.d(8) script"
		fi
		/etc/rc.d/$svc ${_RC_DEBUG} ${_RC_FORCE} $action
		;;
	*)
		usage
		;;
esac
