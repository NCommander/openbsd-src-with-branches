#	$OpenBSD$

PROG=	nfsstat
CFLAGS+=-DNFS
DPADD=  ${LIBKVM}
LDADD= -lkvm
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
