#	$OpenBSD: Makefile,v 1.12 2003/06/22 15:22:43 deraadt Exp $

PROG=	compress
SRCS=	main.c zopen.c gzopen.c
MAN=	compress.1 zmore.1
LINKS=	${BINDIR}/compress ${BINDIR}/uncompress \
	${BINDIR}/compress ${BINDIR}/zcat \
	${BINDIR}/compress ${BINDIR}/gzip \
	${BINDIR}/compress ${BINDIR}/gunzip \
	${BINDIR}/compress ${BINDIR}/gzcat
MLINKS= compress.1 uncompress.1 \
	compress.1 zcat.1 \
	compress.1 gzip.1 \
	compress.1 gunzip.1 \
	compress.1 gzcat.1

LDADD=-lz
DPADD=${LIBZ}

afterinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/zmore ${DESTDIR}${BINDIR}
#	    ${.CURDIR}/zmore ${.CURDIR}/zdiff \
#	    ${.CURDIR}/zforce ${.CURDIR}/gzexe ${.CURDIR}/znew \
#	    ${DESTDIR}${BINDIR}

.include <bsd.prog.mk>
