# $OpenBSD: dot.profile,v 1.6 2009/04/27 05:02:12 deraadt Exp $
#
# sh/ksh initialization

PATH=/sbin:/usr/sbin:/bin:/usr/bin:/usr/X11R6/bin:/usr/local/sbin:/usr/local/bin
export PATH
: ${HOME='/root'}
export HOME
umask 022

if [ -x /usr/bin/tset ]; then
	eval `/usr/bin/tset -sQ '-munknown:?vt220' $TERM`
fi
