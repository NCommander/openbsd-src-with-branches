#	$OpenBSD: Makefile,v 1.1 1997/03/31 03:12:18 weingart Exp $

CFLAGS	+=$(SACFLAGS)
.include "../Makefile.inc"

S	= ${.CURDIR}/../../../..
ZDST=${.OBJDIR}
.include "${S}/lib/libz/Makefile.inc"

all: ${ZLIB}

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
.include <bsd.sys.mk>

