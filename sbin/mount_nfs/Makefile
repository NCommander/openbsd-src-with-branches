#	$OpenBSD: Makefile,v 1.5 1997/04/19 21:26:22 millert Exp $

PROG=	mount_nfs
SRCS=	mount_nfs.c getmntopts.c
MAN=	mount_nfs.8

# Temporarily removed to use own getmntops.c - fvdl
#
#MOUNT=	${.CURDIR}/../mount
#CFLAGS+= -DNFS -I${MOUNT}
#.PATH:	${MOUNT}
CFLAGS+=-DNFS

# NFSKERB is not currently supported
#
#.include <bsd.own.mk>	# For KERBEROS
#
#.if (${KERBEROS} == "yes")
#CFLAGS+=-DNFSKERB
#DPADD+=	${LIBKRB} ${LIBDES}
#LDADD+=	-lkrb -ldes
#.endif

.include <bsd.prog.mk>
