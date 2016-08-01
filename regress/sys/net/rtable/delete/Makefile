#	$OpenBSD$

PROG= 	delete
SRCS= 	main.c util.c rtable.c

REGRESS_TARGETS+=	fullfeed

${REGRESS_TARGETS}: ${PROG}
	${PROG} ${TOPDIR}/inet6-${.TARGET}.txt

.include <bsd.regress.mk>
