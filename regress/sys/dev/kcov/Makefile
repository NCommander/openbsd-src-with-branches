#	$OpenBSD$

PROG=		kcov
WARNINGS=	yes

run-regress-${PROG}: ${PROG}
	if ${SUDO} ./${PROG} -p; then \
		${SUDO} ./${PROG} ${KCOVFLAGS}; \
	else \
		echo "SKIPPED"; \
	fi

.include <bsd.regress.mk>
