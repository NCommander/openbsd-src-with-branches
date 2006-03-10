#      $OpenBSD$

CAP_MKDB?=	cap_mkdb

REGRESS_TARGETS=shortname
CLEANFILES+=	*.db

shortname:
	${CAP_MKDB} -if shortname ${.CURDIR}/shortname.in

.include <bsd.regress.mk>
