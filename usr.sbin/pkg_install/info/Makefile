#	$OpenBSD: Makefile,v 1.1 1996/06/04 07:56:09 niklas Exp $
PROG= pkg_info
CFLAGS+=	${DEBUG} -I${.CURDIR}/../lib

.if exists(${.CURDIR}/../lib/${__objdir})
LDADD+=		-L${.CURDIR}/../lib/${__objdir} -linstall
DPADD+=		${.CURDIR}/../lib/${__objdir}/libinstall.a
.else
LDADD+=		-L${.CURDIR}/../lib -linstall
DPADD+=		${.CURDIR}/../lib/libinstall.a
.endif

SRCS= main.c perform.c show.c

.include <bsd.prog.mk>
