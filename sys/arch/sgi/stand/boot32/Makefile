#	$OpenBSD: Makefile,v 1.4 2007/11/26 10:26:02 deraadt Exp $

.include "${.CURDIR}/../Makefile32.inc"
LDSCRIPT=	${.CURDIR}/ld.script
LDFLAGS+=	${SALDFLAGS} -T ${LDSCRIPT} -e __start -s
PROG=		boot32
.PATH:	${.CURDIR}/../boot
.include "${.CURDIR}/../boot/Makefile"

SRCS+=	ashrdi3.c udivdi3.c umoddi3.c qdivrem.c

${PROG}: $(OBJS) $(LDADD)
	$(LD) $(LDFLAGS) -o ${PROG} $(OBJS) -L${LIBSADIR} ${LIBSA} \
	    -L${LIBZDIR} ${LIBZ}

LINKS=		${BINDIR}/${PROG} ${BINDIR}/boot-IP32

.include <bsd.prog.mk>
