#	$OpenBSD$

PROG=	ccdconfig
MAN=
COPTS+=-Os -DSMALL
.PATH:  ${.CURDIR}/../../../sbin/ccdconfig

DPADD=  ${LIBUTIL}
LDADD=  -lutil

.include <bsd.prog.mk>
