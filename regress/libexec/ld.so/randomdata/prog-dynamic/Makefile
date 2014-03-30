# $OpenBSD$

PROG= prog
LDADD+= -Wl,-rpath,$(LIBAADIR) -L$(LIBAADIR) -laa
DPADD+= $(LIBAA)
.PATH: $(.CURDIR)/../prog

.include <bsd.regress.mk>
