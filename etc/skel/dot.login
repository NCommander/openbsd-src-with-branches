# $OpenBSD: dot.login,v 1.4 2009/01/30 08:42:26 sobrado Exp $
#
# csh login file

if ( ! $?TERMCAP ) then
	if ( $?XTERM_VERSION ) then
		tset -IQ '-munknown:?vt220' $TERM
	else
		tset -Q '-munknown:?vt220' $TERM
	endif
endif

stty	newcrt crterase

set	savehist=100
set	ignoreeof

setenv	EXINIT		'set ai sm noeb'
setenv	HOSTALIASES	 $HOME/.hostaliases

if (-x /usr/games/fortune) /usr/games/fortune
