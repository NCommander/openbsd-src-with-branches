#	$OpenBSD: Makefile,v 1.3 2019/08/04 08:53:14 visa Exp $

NOMAN=

.if ${MACHINE} == "octeon"
PROG=		rdboot
SRCS=		cmd.c disk.c rdboot.c vars.c
LDADD+=		-lutil
LDSTATIC+=	-static
.else
NOPROG=
.endif

.PATH:  ${S}/lib/libsa
SRCS+=  hexdump.c strtoll.c

install:

.include <bsd.prog.mk>
