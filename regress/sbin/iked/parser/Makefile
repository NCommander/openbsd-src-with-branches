#	$OpenBSD: Makefile,v 1.1 2017/05/29 20:59:28 markus Exp $

WARNINGS=Yes

PROG=		test_parser
SRCS=		tests.c common.c test_parser_fuzz.c
IKEOBJS=	ikev2_pld.o imsg_util.o log.o util.o \
		ikev2_map.o eap_map.o
CLEANFILES=	${IKEOBJS}

LDADD+=		-lutil -lcrypto ${IKEOBJS}
DPADD+=		${LIBUTIL} ${LIBCRYPTO}
CFLAGS+=	-I${.CURDIR}/../../../../sbin/iked
CFLAGS+=	-Wno-missing-field-initializers

test_parser: ${IKEOBJS}

${IKEOBJS}:
	cd ${.CURDIR}/../../../../sbin/iked && make $@
	ln -sf ${.OBJDIR}/../../../../sbin/iked/$@ .

LDADD+=		-L${.OBJDIR} -ltest_helper
DPADD+=		libtest_helper.a
CFLAGS+=	-I${.CURDIR}/../test_helper

libtest_helper.a:
	cd ${.CURDIR}/../test_helper && make $@
	ln -sf ${.OBJDIR}/../test_helper/$@ .

.include <bsd.regress.mk>
