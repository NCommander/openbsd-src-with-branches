#	$OpenBSD: Makefile,v 1.2 2001/01/29 02:05:57 niklas Exp $
#	$NetBSD: Makefile,v 1.3 1998/03/02 21:57:38 cgd Exp $

PROG=	unfdpass

CLEANFILES+=file1 file2 output test-sock
REGRESSTARGETS=do-unfdpass

do-unfdpass:
	./unfdpass > output
	cmp -s ${.CURDIR}/expected output

.include <bsd.regress.mk>
