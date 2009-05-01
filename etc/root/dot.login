# $OpenBSD: dot.login,v 1.11 2005/03/30 19:50:07 deraadt Exp $
#
# csh login file

if ( -x /usr/bin/tset ) then
	set noglob histchars=""
	onintr finish
	eval `tset -sQ '-munknown:?vt220' $TERM`
	finish:
	unset noglob histchars
	onintr
endif

if ( `logname` == `whoami` ) then
	echo "Read the afterboot(8) man page for administration advice."
endif
