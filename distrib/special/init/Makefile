#	$OpenBSD: Makefile,v 1.5 1997/02/22 08:38:09 millert Exp $

PROG=	init
MAN=
COPTS+=-Os
CFLAGS+=-DDEFAULT_STATE=single_user
DPADD=	${LIBUTIL} 
LDADD=	-lutil 
.PATH:  ${.CURDIR}/../../../sbin/init

.include <bsd.prog.mk>
