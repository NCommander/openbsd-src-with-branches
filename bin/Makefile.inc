#	$OpenBSD: Makefile.inc,v 1.5 2014/07/10 12:55:14 tedu Exp $

BINDIR?=	/bin
LDSTATIC=	${STATIC}
COPTS+=-Werror-implicit-function-declaration
