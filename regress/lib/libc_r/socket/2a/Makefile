# $OpenBSD: Makefile,v 1.2 2002/01/03 01:43:24 art Exp $

PROG=	socket2a
CFLAGS+= -I${.CURDIR}/../../include

REGRESSTARGETS=dummy

dummy: ${PROG}
	@cd ${.CURDIR}/../2; \
	if test -d ${__objdir} ; then \
		cd ${__objdir} ; \
	fi; \
	ln -sf ${.OBJDIR}/${PROG}
	# Nothing here so far...

.include <bsd.regress.mk>

