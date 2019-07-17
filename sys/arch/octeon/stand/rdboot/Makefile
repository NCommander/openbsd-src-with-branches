#	$OpenBSD$

NOMAN=

.if ${MACHINE} == "octeon"
PROG=		rdboot
SRCS=		cmd.c disk.c rdboot.c vars.c
LDADD+=		-lutil
LDSTATIC+=	-static
.else
NOPROG=
.endif

install:

.include <bsd.prog.mk>
