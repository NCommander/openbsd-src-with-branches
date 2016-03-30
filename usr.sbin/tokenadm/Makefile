#	$OpenBSD: Makefile,v 1.3 2010/10/15 10:18:42 jsg Exp $

PROG=	tokenadm
SRCS=	tokenadm.c init.c tokendb.c
MAN=	tokenadm.8
LDADD+=	-lcrypto
DPADD=	${LIBCRYPTO}

CFLAGS+=-I${.CURDIR}/../../libexec/login_token
.PATH:	${.CURDIR}/../../libexec/login_token

TOKENS=	activ crypto snk

BINMODE=500

afterinstall:
	for i in ${TOKENS} ; do \
	    cd ${DESTDIR}${BINDIR} && \
	    rm -f $${i}adm && \
	    ln ${PROG} $${i}adm ; \
	done

.include <bsd.prog.mk>
