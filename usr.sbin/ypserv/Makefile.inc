#	$OpenBSD: Makefile.inc,v 1.3 1997/09/21 11:44:38 deraadt Exp $

BINDIR?=	/usr/sbin
.if (${MACHINE_ARCH} == "alpha")
CFLAGS:=	${CFLAGS:S/^-O2$/-O/}
.endif
