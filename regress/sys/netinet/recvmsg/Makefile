#	$OpenBSD$

PROG=		recvmsg
WARNINGS=	yes

REGRESS_TARGETS+=	dgram raw

dgram: ${PROG}
	./${PROG} -4d

raw: ${PROG}
	${SUDO} ./${PROG} -4r

.include <bsd.regress.mk>
