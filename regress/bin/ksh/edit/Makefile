# $OpenBSD: Makefile,v 1.3 2017/07/06 19:40:18 bluhm Exp $

REGRESS_TARGETS=	emacs vi

KSH=	/bin/ksh

PROG=		edit
CFLAGS+=	-Wall
LDADD+=		-lutil

emacs: edit
	sh ${.CURDIR}/emacs.sh ${.CURDIR} ${KSH}

vi: edit
	sh ${.CURDIR}/vi.sh ${.CURDIR} ${KSH}

.include <bsd.regress.mk>
