#	$OpenBSD: Makefile,v 1.4 2023/06/22 19:23:27 tb Exp $

PROG = symbols

#include <bsd.own.mk>

DPADD=		${LIBCRYPTO} ${LIBSSL}
LDFLAGS+=	-lcrypto -lssl
LDFLAGS+=	-Wl,--no-allow-shlib-undefined
CFLAGS+=	-Wno-deprecated-declarations

CLEANFILES+= symbols.c symbols.c.tmp

symbols.c: symbols.awk ../../../../lib/libssl/Symbols.list
	awk -f ${.CURDIR}/symbols.awk \
		< ${BSDSRCDIR}/lib/libssl/Symbols.list > $@.tmp && \
	mv -f $@.tmp $@

run-regress-symbols: symbols
	./symbols 2>/dev/null

.include <bsd.regress.mk>
