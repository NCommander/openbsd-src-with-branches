#	$OpenBSD: Makefile,v 1.1.2.2 1996/12/15 15:49:35 mickey Exp $

PROG=	boot
SRCS=	srt0.S boot.c cmd.c conf.c version.c
CFLAGS=-I${.CURDIR}/../../../../lib/libsa -I${.CURDIR}/../libsa
AFLAGS+=-DREL=$(REL) #-Wa,-a
CFLAGS+=-Wall -O
LDFLAGS=-Wl,-T$(REL),-x,-z -e start_boot
LDFLAGS+=-static -nostdlib
MAN=	boot.8

LDADD=	-L../libsa -lsa -L../libz -lz
DPADD=	../libsa/libsa.a ../libz/libz.a

.include <bsd.prog.mk>

.ifdef NO_NET
CPPFLAGS+=-DNO_NET
.endif
