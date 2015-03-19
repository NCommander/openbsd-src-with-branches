# $OpenBSD: Makefile,v 1.1.1.1 2012/08/16 17:21:05 matthew Exp $

PROG= prog
LDSTATIC= ${STATIC}
LDADD+= -L$(LIBAADIR) -laa
DPADD+= $(LIBAA)
.PATH: $(.CURDIR)/../prog

.include <bsd.regress.mk>
