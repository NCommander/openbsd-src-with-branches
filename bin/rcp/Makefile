#	$OpenBSD: Makefile,v 1.9 2003/05/14 08:01:02 mho Exp $

PROG=	rcp
SRCS=	rcp.c util.c
BINOWN=	root
BINMODE=555
#INSTALLFLAGS=-fschg

.include <bsd.own.mk>	# For KERBEROS

#.if (${KERBEROS:L} == "yes")
#.PATH:	${.CURDIR}/../../usr.bin/rsh
#SRCS+=	krcmd.c kcmd.c
#CFLAGS+=-DKERBEROS
#DPADD+=	${LIBKRB} ${LIBDES}
#LDADD+=	-lkrb -ldes
#.endif

.include <bsd.prog.mk>
