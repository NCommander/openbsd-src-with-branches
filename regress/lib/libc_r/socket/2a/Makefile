# $OpenBSD: Makefile,v 1.3 2002/01/04 13:56:36 art Exp $

PROG=	socket2a
CFLAGS+= -I${.CURDIR}/../../include

REGRESS_TARGETS=dummy

dummy: ${PROG}
	@cd ${.CURDIR}/../2; \
	if test -d ${__objdir} ; then \
		cd ${__objdir} ; \
	fi; \
	ln -sf ${.OBJDIR}/${PROG}
	# Nothing here so far...

.include <bsd.regress.mk>

