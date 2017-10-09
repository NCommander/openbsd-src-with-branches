# $OpenBSD$

PROG= prog
CFLAGS+= -fPIE
LDFLAGS+= -pie
LDADD+= -Wl,-rpath,$(LIBAADIR) -L$(LIBAADIR) -laa
DPADD+= $(LIBAA)
.PATH: $(.CURDIR)/../prog

.include <bsd.regress.mk>
