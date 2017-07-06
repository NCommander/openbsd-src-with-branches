# $OpenBSD$

REGRESS_TARGETS=	emacs vi

PROG=		edit
CFLAGS+=	-Wall
LDADD+=		-lutil

emacs: edit
	sh ${.CURDIR}/emacs.sh ${.CURDIR}

vi: edit
	sh ${.CURDIR}/vi.sh ${.CURDIR}

.include <bsd.regress.mk>
