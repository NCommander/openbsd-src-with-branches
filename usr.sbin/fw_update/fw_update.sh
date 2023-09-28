#!/bin/ksh
#	$OpenBSD: fw_update.sh,v 1.48 2023/09/28 00:45:22 afresh1 Exp $
#
# Copyright (c) 2021,2023 Andrew Hewus Fresh <afresh1@openbsd.org>
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

set -o errexit -o pipefail -o nounset -o noclobber -o noglob
set +o monitor
export PATH=/usr/bin:/bin:/usr/sbin:/sbin

CFILE=SHA256.sig
DESTDIR=${DESTDIR:-}
FWPATTERNS="${DESTDIR}/usr/share/misc/firmware_patterns"

VNAME=${VNAME:-$(sysctl -n kern.osrelease)}
VERSION=${VERSION:-"${VNAME%.*}${VNAME#*.}"}

HTTP_FWDIR="$VNAME"
VTYPE=$( sed -n "/^OpenBSD $VNAME\([^ ]*\).*$/s//\1/p" \
    /var/run/dmesg.boot | sed '$!d' )
[ "$VTYPE" = -current ] && HTTP_FWDIR=snapshots

FWURL=http://firmware.openbsd.org/firmware/${HTTP_FWDIR}
FWPUB_KEY=${DESTDIR}/etc/signify/openbsd-${VERSION}-fw.pub

DRYRUN=false
integer VERBOSE=0
DELETE=false
DOWNLOAD=true
INSTALL=true
LOCALSRC=
ENABLE_SPINNER=false
[ -t 1 ] && ENABLE_SPINNER=true

integer STATUS_FD=1
integer WARN_FD=2
FD_DIR=

unset FTPPID
unset LOCKPID
unset FWPKGTMP
REMOVE_LOCALSRC=false

status() { echo -n "$*" >&"$STATUS_FD"; }
warn()   { echo    "$*" >&"$WARN_FD"; }

cleanup() {
	set +o errexit # ignore errors from killing ftp

	if [ -d "$FD_DIR" ]; then
		echo "" >&"$STATUS_FD"
		exec 4>&-

		[ -s "$FD_DIR/status" ] && cat "$FD_DIR/status"
		[ -s "$FD_DIR/warn"   ] && cat "$FD_DIR/warn" >&2

		rm -rf "$FD_DIR"
	fi

	[ "${FTPPID:-}" ] && kill -TERM -"$FTPPID" 2>/dev/null
	[ "${LOCKPID:-}" ] && kill -TERM -"$LOCKPID" 2>/dev/null
	[ "${FWPKGTMP:-}" ] && rm -rf "$FWPKGTMP"
	"$REMOVE_LOCALSRC" && rm -rf "$LOCALSRC"
	[ -e "$CFILE" ] && [ ! -s "$CFILE" ] && rm -f "$CFILE"
}
trap cleanup EXIT

tmpdir() {
	local _i=1 _dir

	# The installer lacks mktemp(1), do it by hand
	if [ -x /usr/bin/mktemp ]; then
		_dir=$( mktemp -d "${1}-XXXXXXXXX" )
	else
		until _dir="${1}.$_i.$RANDOM" && mkdir -- "$_dir" 2>/dev/null; do
		    ((++_i < 10000)) || return 1
		done
	fi

	echo "$_dir"
}

spin() {
	if ! "$ENABLE_SPINNER"; then
		sleep 1
		return 0
	fi

	{
		for p in '/' '-' '\\' '|' '/' '-' '\\' '|'; do
			echo -n "$p"'\010'
			sleep 0.125
		done
	}>/dev/tty
}

fetch() {
	local _src="${FWURL}/${1##*/}" _dst=$1 _user=_file _exit _error=''

	# The installer uses a limited doas(1) as a tiny su(1)
	set -o monitor # make sure ftp gets its own process group
	(
	_flags=-vm
	case "$VERBOSE" in
		0|1) _flags=-VM ;;
		  2) _flags=-Vm ;;
	esac
	if [ -x /usr/bin/su ]; then
		exec /usr/bin/su -s /bin/ksh "$_user" -c \
		    "/usr/bin/ftp -N '${0##/}' -D 'Get/Verify' $_flags -o- '$_src'" > "$_dst"
	else
		exec /usr/bin/doas -u "$_user" \
		    /usr/bin/ftp -N "${0##/}" -D 'Get/Verify' $_flags -o- "$_src" > "$_dst"
	fi
	) & FTPPID=$!
	set +o monitor

	SECONDS=0
	_last=0
	while kill -0 -"$FTPPID" 2>/dev/null; do
		if [[ $SECONDS -gt 12 ]]; then
			set -- $( ls -ln "$_dst" 2>/dev/null )
			if [[ $_last -ne $5 ]]; then
				_last=$5
				SECONDS=0
				spin
			else
				kill -INT -"$FTPPID" 2>/dev/null
				_error=" (timed out)"
			fi
		else
			spin
		fi
	done

	set +o errexit
	wait "$FTPPID"
	_exit=$?
	set -o errexit

	unset FTPPID

	if [ "$_exit" -ne 0 ]; then
		rm -f "$_dst"
		warn "Cannot fetch $_src$_error"
		return 1
	fi

	return 0
}

# If we fail to fetch the CFILE, we don't want to try again
# but we might be doing this in a subshell so write out
# a blank file indicating failure.
check_cfile() {
	if [ -e "$CFILE" ]; then
		[ -s "$CFILE" ] || return 1
		return 0
	fi
	if ! fetch_cfile; then
		echo -n > "$CFILE"
		return 1
	fi
	return 0
}

fetch_cfile() {
	if "$DOWNLOAD"; then
		set +o noclobber # we want to get the latest CFILE
		fetch "$CFILE" || return 1
		set -o noclobber
		! signify -qVep "$FWPUB_KEY" -x "$CFILE" -m "$CFILE" &&
		    warn "Signature check of SHA256.sig failed" &&
		    rm -f "$CFILE" && return 1
	elif [ ! -e "$CFILE" ]; then
		warn "${0##*/}: $CFILE: No such file or directory"
		return 1
	fi

	return 0
}

verify() {
	check_cfile || return 1
	# The installer sha256 lacks -C, do it by hand
	if ! grep -Fqx "SHA256 (${1##*/}) = $( /bin/sha256 -qb "$1" )" "$CFILE"
	then
		((VERBOSE != 1)) && warn "Checksum test for ${1##*/} failed."
		return 1
	fi

	return 0
}

# When verifying existing files that we are going to re-download
# if VERBOSE is 0, don't show the checksum failure of an existing file.
verify_existing() {
	local _v=$VERBOSE
	check_cfile || return 1

	((_v == 0)) && "$DOWNLOAD" && _v=1
	( VERBOSE=$_v verify "$@" )
}

firmware_in_dmesg() {
	local IFS
	local _d _m _dmesgtail _last='' _nl='
'

	# The dmesg can contain multiple boots, only look in the last one
	_dmesgtail="$( echo ; sed -n 'H;/^OpenBSD/h;${g;p;}' /var/run/dmesg.boot )"

	grep -v '^[[:space:]]*#' "$FWPATTERNS" |
	    while read -r _d _m; do
		[ "$_d" = "$_last" ]  && continue
		[ "$_m" ]             || _m="${_nl}${_d}[0-9] at "
		[ "$_m" = "${_m#^}" ] || _m="${_nl}${_m#^}"

		IFS='*'
		set -- $_m
		unset IFS

		case $# in
		    1|2|3) [[ $_dmesgtail = *$1*([!$_nl])${2-}*([!$_nl])${3-}* ]] || continue;;
		    *) warn "${0##*/}: Bad pattern '${_m#$_nl}' in $FWPATTERNS"; exit 1 ;;
		esac

		echo "$_d"
		_last="$_d"
	    done
}

firmware_filename() {
	check_cfile || return 1
	sed -n "s/.*(\($1-firmware-.*\.tgz\)).*/\1/p" "$CFILE" | sed '$!d'
}

firmware_devicename() {
	local _d="${1##*/}"
	_d="${_d%-firmware-*}"
	echo "$_d"
}

lock_db() {
	[ "${LOCKPID:-}" ] && return 0

	# The installer doesn't have perl, so we can't lock there
	[ -e /usr/bin/perl ] || return 0

	set -o monitor
	perl <<-'EOL' |&
		no lib ('/usr/local/libdata/perl5/site_perl');
		use v5.36;
		use OpenBSD::PackageInfo qw< lock_db >;

		$|=1;

		$0 = "fw_update: lock_db";
		lock_db(0);

		say $$;

		# Wait for STDOUT to be readable, which won't happen
		# but if our parent exits unexpectedly it will close.
		my $rin = '';
		vec($rin, fileno(STDOUT), 1) = 1;
		select $rin, '', '', undef;
EOL
	set +o monitor

	read -rp LOCKPID

	return 0
}

installed_firmware() {
	local _pre="$1" _match="$2" _post="$3" _firmware _fw
	set -sA _firmware -- $(
	    set +o noglob
	    grep -Fxl '@option firmware' \
		"${DESTDIR}/var/db/pkg/"$_pre"$_match"$_post"/+CONTENTS" \
		2>/dev/null || true
	    set -o noglob
	)

	[ "${_firmware[*]:-}" ] || return 0
	for _fw in "${_firmware[@]}"; do
		_fw="${_fw%/+CONTENTS}"
		echo "${_fw##*/}"
	done
}

detect_firmware() {
	local _devices _last='' _d

	set -sA _devices -- $(
	    firmware_in_dmesg
	    for _d in $( installed_firmware '*' '-firmware-' '*' ); do
		firmware_devicename "$_d"
	    done
	)

	[ "${_devices[*]:-}" ] || return 0
	for _d in "${_devices[@]}"; do
		[ "$_last" = "$_d" ] && continue
		echo "$_d"
		_last="$_d"
	done
}

add_firmware () {
	local _f="${1##*/}" _m="${2:-Install}" _pkgname
	FWPKGTMP="$( tmpdir "${DESTDIR}/var/db/pkg/.firmware" )"
	local _flags=-vm
	case "$VERBOSE" in
		0|1) _flags=-VM ;;
		2|3) _flags=-Vm ;;
	esac

	ftp -N "${0##/}" -D "$_m" "$_flags" -o- "file:${1}" |
		tar -s ",^\+,${FWPKGTMP}/+," \
		    -s ",^firmware,${DESTDIR}/etc/firmware," \
		    -C / -zxphf - "+*" "firmware/*"

	_pkgname="$( sed -n '/^@name /{s///p;q;}' "${FWPKGTMP}/+CONTENTS" )"
	if [ ! "$_pkgname" ]; then
		echo "Failed to extract name from $1, partial install" 2>&1
		rm -rf "$FWPKGTMP"
		unset FWPKGTMP
		return 1
	fi

	ed -s "${FWPKGTMP}/+CONTENTS" <<EOL
/^@comment pkgpath/ -1a
@option manual-installation
@option firmware
@comment install-script
.
w
EOL

	chmod 755 "$FWPKGTMP"
	mv "$FWPKGTMP" "${DESTDIR}/var/db/pkg/${_pkgname}"
	unset FWPKGTMP
}

remove_files() {
	local _r
	# Use rm -f, not removing files/dirs is probably not worth failing over
	for _r in "$@" ; do
		if [ -d "$_r" ]; then
			# The installer lacks rmdir,
			# but we only want to remove empty directories.
			set +o noglob
			[ "$_r/*" = "$( echo "$_r"/* )" ] && rm -rf "$_r"
			set -o noglob
		else
			rm -f "$_r"
		fi
	done
}

delete_firmware() {
	local _cwd _pkg="$1" _pkgdir="${DESTDIR}/var/db/pkg"

	# TODO: Check hash for files before deleting
	((VERBOSE > 2)) && echo -n "Uninstall $_pkg ..."
	_cwd="${_pkgdir}/$_pkg"

	if [ ! -e "$_cwd/+CONTENTS" ] ||
	    ! grep -Fxq '@option firmware' "$_cwd/+CONTENTS"; then
		warn "${0##*/}: $_pkg does not appear to be firmware"
		return 2
	fi

	set -A _remove -- "${_cwd}/+CONTENTS" "${_cwd}"

	while read -r _c _g; do
		case $_c in
		@cwd) _cwd="${DESTDIR}$_g"
		  ;;
		@*) continue
		  ;;
		*) set -A _remove -- "$_cwd/$_c" "${_remove[@]}"
		  ;;
		esac
	done < "${_pkgdir}/${_pkg}/+CONTENTS"

	remove_files "${_remove[@]}"

	((VERBOSE > 2)) && echo " done."

	return 0
}

unregister_firmware() {
	local _d="$1" _pkgdir="${DESTDIR}/var/db/pkg" _fw

	set -A installed -- $( installed_firmware '' "$d-firmware-" '*' )
	if [ "${installed:-}" ]; then
		for _fw in "${installed[@]}"; do
			((VERBOSE)) && echo "Unregister $_fw"
			"$DRYRUN" && continue
			remove_files \
			    "$_pkgdir/$_fw/+CONTENTS" \
			    "$_pkgdir/$_fw/+DESC" \
			    "$_pkgdir/$_fw/"
		done
		return 0
	fi

	return 1
}

usage() {
	echo "usage: ${0##*/} [-adFnv] [-p path] [driver | file ...]"
	exit 1
}

ALL=false
OPT_F=
while getopts :adFnp:v name
do
	case "$name" in
	a) ALL=true ;;
	d) DELETE=true ;;
	F) OPT_F=true ;;
	n) DRYRUN=true ;;
	p) LOCALSRC="$OPTARG" ;;
	v) ((++VERBOSE)) ;;
	:)
	    warn "${0##*/}: option requires an argument -- -$OPTARG"
	    usage
	    ;;
	?)
	    warn "${0##*/}: unknown option -- -$OPTARG"
	    usage
	    ;;
	esac
done
shift $((OPTIND - 1))

# Progress bars, not spinner When VERBOSE > 1
((VERBOSE > 1)) && ENABLE_SPINNER=false

if [ "$LOCALSRC" ]; then
	if [[ $LOCALSRC = @(ftp|http?(s))://* ]]; then
		FWURL="${LOCALSRC}"
		LOCALSRC=
	else
		LOCALSRC="${LOCALSRC#file:}"
		! [ -d "$LOCALSRC" ] &&
		    warn "The path must be a URL or an existing directory" &&
		    exit 1
	fi
fi

# "Download only" means local dir and don't install
if [ "$OPT_F" ]; then
	INSTALL=false
	LOCALSRC="${LOCALSRC:-.}"

	# Always check for latest CFILE and so latest firmware
	if [ -e "$LOCALSRC/$CFILE" ]; then
		mv "$LOCALSRC/$CFILE" "$LOCALSRC/$CFILE-OLD"
		if check_cfile; then
			rm -f "$LOCALSRC/$CFILE-OLD"
		else
			mv "$LOCALSRC/$CFILE-OLD" "$LOCALSRC/$CFILE"
			warn "Using existing $CFILE"
		fi
	fi
elif [ "$LOCALSRC" ]; then
	DOWNLOAD=false
fi

if [ -x /usr/bin/id ] && [ "$(/usr/bin/id -u)" != 0 ]; then
	warn "need root privileges"
	exit 1
fi

set -sA devices -- "$@"

# In the normal case, we output the status line piecemeal
# so we save warnings to output at the end to not disrupt
# the single line status.
# Actual errors from things like ftp will stil interrupt,
# but it's impossible to know if it's a message people need
# to see now or something that can wait.
# In the verbose case, we instead print out single lines
# or progress bars for each thing we are doing,
# so instead we save up the final status line for the end.
FD_DIR="$( tmpdir "${DESTDIR}/tmp/${0##*/}-fd" )"
if ((VERBOSE)); then
	exec 4>"${FD_DIR}/status"
	STATUS_FD=4
else
	exec 4>"${FD_DIR}/warn"
	WARN_FD=4
fi

status "${0##*/}:"

if "$DELETE"; then
	[ "$OPT_F" ] && warn "Cannot use -F and -d" && usage
	lock_db

	# Show the "Uninstall" message when just deleting not upgrading
	((VERBOSE)) && VERBOSE=3

	set -A installed
	if [ "${devices[*]:-}" ]; then
		"$ALL" && warn "Cannot use -a and devices/files" && usage

		set -A installed -- $(
		    for d in "${devices[@]}"; do
			f="${d##*/}"  # only care about the name
			f="${f%.tgz}" # allow specifying the package name
			[ "$( firmware_devicename "$f" )" = "$f" ] && f="$f-firmware"

			set -A i -- $( installed_firmware '' "$f-" '*' )

			if [ "${i[*]:-}" ]; then
				echo "${i[@]}"
			else
				warn "No firmware found for '$d'"
			fi
		    done
		)
	elif "$ALL"; then
		set -A installed -- $( installed_firmware '*' '-firmware-' '*' )
	fi

	status " delete "

	comma=''
	if [ "${installed:-}" ]; then
		for fw in "${installed[@]}"; do
			status "$comma$( firmware_devicename "$fw" )"
			comma=,
			if "$DRYRUN"; then
				((VERBOSE)) && echo "Delete $fw"
			else
				delete_firmware "$fw" || continue
			fi
		done
	fi

	[ "$comma" ] || status none

	exit
fi

if [ ! "$LOCALSRC" ]; then
	LOCALSRC="$( tmpdir "${DESTDIR}/tmp/${0##*/}" )"
	REMOVE_LOCALSRC=true
fi

CFILE="$LOCALSRC/$CFILE"

if [ "${devices[*]:-}" ]; then
	"$ALL" && warn "Cannot use -a and devices/files" && usage
else
	((VERBOSE > 1)) && echo -n "Detect firmware ..."
	set -sA devices -- $( detect_firmware )
	((VERBOSE > 1)) &&
	    { [ "${devices[*]:-}" ] && echo " found." || echo " done." ; }
fi


set -A add ''
set -A update ''
kept=''
unregister=''

if [ "${devices[*]:-}" ]; then
	lock_db
	for f in "${devices[@]}"; do
		d="$( firmware_devicename "$f" )"

		verify_existing=true
		if [ "$f" = "$d" ]; then
			f=$( firmware_filename "$d" ) || continue
			if [ ! "$f" ]; then
				if "$INSTALL" && unregister_firmware "$d"; then
					unregister="$unregister,$d"
				else
					warn "Unable to find firmware for $d"
				fi
				continue
			fi
		elif ! "$INSTALL" && ! grep -Fq "($f)" "$CFILE" ; then
			warn "Cannot download local file $f"
			exit 1
		else
			# Don't verify files specified on the command-line
			verify_existing=false
		fi

		set -A installed
		if "$INSTALL"; then
			set -A installed -- \
			    $( installed_firmware '' "$d-firmware-" '*' )

			if [ "${installed[*]:-}" ]; then
				for i in "${installed[@]}"; do
					if [ "${f##*/}" = "$i.tgz" ]; then
						((VERBOSE > 2)) \
						    && echo "Keep $i"
						kept="$kept,$d"
						continue 2
					fi
				done
			fi
		fi

		# Fetch an unqualified file into LOCALSRC
		# if it doesn't exist in the current directory.
		if [ "$f" = "${f##/}" ] && [ ! -e "$f" ]; then
			f="$LOCALSRC/$f"
		fi

		if "$verify_existing" && [ -e "$f" ]; then
			pending_status=false
			if ((VERBOSE == 1)); then
				echo -n "Verify ${f##*/} ..."
				pending_status=true
			elif ((VERBOSE > 1)) && ! "$INSTALL"; then
				echo "Keep/Verify ${f##*/}"
			fi

			if "$DRYRUN" || verify_existing "$f"; then
				"$pending_status" && echo " done."
				if ! "$INSTALL"; then
					kept="$kept,$d"
					continue
				fi
			elif "$DOWNLOAD"; then
				"$pending_status" && echo " failed."
				((VERBOSE > 1)) && echo "Refetching $f"
				rm -f "$f"
			else
				"$pending_status" && echo " failed."
				continue
			fi
		fi

		if [ "${installed[*]:-}" ]; then
			set -A update -- "${update[@]}" "$f"
		else
			set -A add -- "${add[@]}" "$f"
		fi

	done
fi

if "$INSTALL"; then
	status " add "
	action=Install
else
	status " download "
	action=Download
fi

comma=''
[ "${add[*]}" ] || status none
for f in "${add[@]}" _update_ "${update[@]}"; do
	[ "$f" ] || continue
	if [ "$f" = _update_ ]; then
		comma=''
		"$INSTALL" || continue
		action=Update
		status "; update "
		[ "${update[*]}" ] || status none
		continue
	fi
	d="$( firmware_devicename "$f" )"
	status "$comma$d"
	comma=,

	pending_status=false
	if [ -e "$f" ]; then
		if "$DRYRUN"; then
			((VERBOSE)) && echo "$action ${f##*/}"
		else
			if ((VERBOSE == 1)); then
				echo -n "Install ${f##*/} ..."
				pending_status=true
			fi
		fi
	elif "$DOWNLOAD"; then
		if "$DRYRUN"; then
			((VERBOSE)) && echo "Get/Verify ${f##*/}"
		else
			if ((VERBOSE == 1)); then
				echo -n "Get/Verify ${f##*/} ..."
				pending_status=true
			fi
			fetch  "$f" &&
			verify "$f" || {
				if "$pending_status"; then
					echo " failed."
				elif ! ((VERBOSE)); then
					status " failed (${f##*/})"
				fi
				continue
			}
		fi
	elif "$INSTALL"; then
		warn "Cannot install ${f##*/}, not found"
		continue
	fi

	if ! "$INSTALL"; then
		"$pending_status" && echo " done."
		continue
	fi

	if ! "$DRYRUN"; then
		if [ "$action" = Update ]; then
			for i in $( installed_firmware '' "$d-firmware-" '*' )
			do
				delete_firmware "$i" || {
					if "$pending_status"; then
						echo " failed."
					elif ! ((VERBOSE)); then
						status " failed ($i)"
					fi
					continue
				}
			done
		fi

		add_firmware "$f" "$action" || {
			if "$pending_status"; then
				echo " failed."
			elif ! ((VERBOSE)); then
				status " failed (${f##*/})"
			fi
			continue
		}
	fi

	if "$pending_status"; then
		if [ "$action" = Install ]; then
			echo " installed."
		else
			echo " updated."
		fi
	fi
done

[ "$unregister" ] && status "; unregister ${unregister:#,}"
[ "$kept"       ] && status "; keep ${kept:#,}"

exit 0
