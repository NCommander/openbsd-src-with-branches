#	$OpenBSD: Makefile.inc,v 1.3 2016/05/07 19:05:22 guenther Exp $

LDADD+= -Wl,--no-execute-only

CERROR=	cerror.S
