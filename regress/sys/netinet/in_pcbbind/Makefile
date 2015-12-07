# $OpenBSD$

PROG	 = runtest
PORT	 = 23000
IFADDR	!= route -n show -inet | awk '$$8 !~ "lo0" && $$3 ~ /U.*l/ { print $$1; exit }'

run-regress-runtest: ${PROG}
	${.OBJDIR}/${PROG} ${PORT} ${IFADDR}

.include <bsd.regress.mk>
