#	$OpenBSD: Makefile,v 1.1.2.1 1996/12/03 13:13:41 mickey Exp $

CFLAGS+=-O2 -DNO_IDTR -Wall

S	= ${.CURDIR}/../../../..
SAREL=/
SADST=.
.include "${S}/lib/libsa/Makefile.inc"

all: ${SALIB}

.include <bsd.obj.mk>
.include <bsd.dep.mk>
.include <bsd.subdir.mk>
.include <bsd.sys.mk>
