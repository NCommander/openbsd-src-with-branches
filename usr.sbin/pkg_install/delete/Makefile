#	$OpenBSD: Makefile,v 1.1 1996/06/04 07:56:07 niklas Exp $
PROG= pkg_delete
CFLAGS+=	${DEBUG} -I${.CURDIR}/../lib

.if exists(${.CURDIR}/../lib/${__objdir})
LDADD+=		-L${.CURDIR}/../lib/${__objdir} -linstall
DPADD+=		${.CURDIR}/../lib/${__objdir}/libinstall.a
.else
LDADD+=		-L${.CURDIR}/../lib -linstall
DPADD+=		${.CURDIR}/../lib/libinstall.a
.endif

SRCS= main.c perform.c

.include <bsd.prog.mk>
