# $OpenBSD$

PROG= prog
LDADD+= -L$(LIBAADIR) -laa
DPADD+= $(LIBAA)
LDFLAGS+= -static
.PATH: $(.CURDIR)/../prog

.include <bsd.regress.mk>
