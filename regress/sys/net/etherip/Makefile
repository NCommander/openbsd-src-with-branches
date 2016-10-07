#	$OpenBSD$

REGRESS_TARGETS=	etherip_1
REGRESS_ROOT_TARGETS=	etherip_1

RDOMAINS=		11 12
IFACE_NUMS=		11 12

etherip_1:
	${SUDO} ksh ${.CURDIR}/$@.sh -R "${RDOMAINS}" -I "${IFACE_NUMS}"

.include <bsd.regress.mk>
