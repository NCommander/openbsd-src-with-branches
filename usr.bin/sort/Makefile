#	$OpenBSD: Makefile,v 1.2 1997/09/21 11:50:57 deraadt Exp $

PROG=	sort
SRCS=	append.c fields.c files.c fsort.c init.c msort.c sort.c tmp.c

# Work around gcc optimization bug on sh
.if (${MACHINE_ARCH} == sh)
COPTS+= -O1
.endif

.include <bsd.prog.mk>
