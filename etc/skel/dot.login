# $OpenBSD: dot.login,v 1.6 2015/12/15 16:37:58 deraadt Exp $
#
# csh login file

if ( ! $?TERMCAP ) then
	tset -IQ '-munknown:?vt220' $TERM
endif

stty	newcrt crterase

set	savehist=100
set	ignoreeof

setenv	EXINIT		'set ai sm noeb'

if (-x /usr/games/fortune) /usr/games/fortune
