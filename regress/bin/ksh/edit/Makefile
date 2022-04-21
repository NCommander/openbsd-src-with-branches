# $OpenBSD: Makefile,v 1.4 2017/08/22 20:14:57 anton Exp $

KSH=	/bin/ksh

PROG=		edit
CFLAGS+=	-Wall
LDADD+=		-lutil

REGRESS_SETUP_ONCE=	${PROG}

REGRESS_TARGETS+=	emacs
REGRESS_TARGETS+=	vi

emacs:
	sh ${.CURDIR}/emacs.sh ${.CURDIR} ${KSH}

vi:
	sh ${.CURDIR}/vi.sh ${.CURDIR} ${KSH}

.include <bsd.regress.mk>
