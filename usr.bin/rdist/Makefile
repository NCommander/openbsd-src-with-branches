#	$OpenBSD: Makefile,v 1.11 2001/08/31 23:31:30 millert Exp $


PROG=	rdist
CFLAGS+=-I. -DOS_H=\"os-openbsd.h\"
SRCS=	gram.y child.c client.c common.c distopt.c docmd.c expand.c \
	isexec.c lookup.c message.c rdist.c
CLEANFILES+=gram.c y.tab.h

LDADD=	-lcompat
DPADD=	${LIBCOMPAT}

.include <bsd.prog.mk>

.if ${ELF_TOOLCHAIN} == "yes"
CFLAGS+=-DEXE_TYPE=EXE_ELF
.endif

.depend: gram.c
