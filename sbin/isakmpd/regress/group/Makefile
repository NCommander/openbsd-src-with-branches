#	$OpenBSD: Makefile,v 1.2 1998/11/15 00:44:09 niklas Exp $
#	$EOM: Makefile,v 1.6 1998/12/21 00:41:53 niklas Exp $

# Test Group:

PROG=		grouptest
SRCS=		math_2n.c math_ec2n.c math_group.c grouptest.c util.c \
		log.c sysdep.c gmp_util.c
.PATH:		${.CURDIR}/../../
NOMAN=
TOPOBJ!=	cd ${.CURDIR}/../..; printf "all:\n\t@pwd\n" |${MAKE} -f-
CFLAGS+=	-I${TOPOBJ} -I${.CURDIR}/../../ -Wall
LDADD+=		-lgmp
DPADD+=		${LIBGMP}
DEBUG=		-g

.include <bsd.prog.mk>
