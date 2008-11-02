#	$OpenBSD:$ 

PROG=	cansig
CFLAGS+=-Wall

t-cansig: ${PROG}
	${SUDO} ./${PROG}

REGRESS_TARGETS=t-cansig
REGRESS_ROOT_TARGETS=${REGRESS_TARGETS}

.include <bsd.regress.mk>
