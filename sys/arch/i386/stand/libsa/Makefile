#	$OpenBSD$

CFLAGS+=-O2 -DNO_IDTR

S	= ${.CURDIR}/../../../..
SAREL=/
SADST=.
.include "${S}/lib/libsa/Makefile.inc"

all: ${SALIB}

.include <bsd.obj.mk>
.include <bsd.dep.mk>
.include <bsd.subdir.mk>
.include <bsd.sys.mk>
