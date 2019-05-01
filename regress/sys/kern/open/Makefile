#	$OpenBSD$

PROG=		open
WARNINGS=	yes

REGRESS_TARGETS+=	clone-device

clone-device: ${PROG}
	${SUDO} ./${PROG}

.include <bsd.regress.mk>
