#	$OpenBSD$

BINGRP?=	games
.if defined(HIDEGAME)
BINDIR?=	/usr/games/hide
BINMODE?=	550
.else
BINDIR?=	/usr/games
.endif
