#	$OpenBSD: Makefile,v 1.5 2003/07/19 00:03:51 david Exp $
#	$NetBSD: Makefile,v 1.1 1996/02/21 02:45:47 jtk Exp $

MAN+=	x86_64_get_ldt.2 \
	x86_64_iopl.2 x86_64_get_ioperm.2
MANSUBDIR=amd64
MLINKS+=x86_64_get_ldt.2 x86_64_set_ldt.2 \
	x86_64_get_ioperm.2 x86_64_set_ioperm.2

.if ${MACHINE_ARCH} == "x86_64"
.PATH: ${LIBC}/x86_64
NOPIC=
SRCS+=	x86_64_get_ldt.c x86_64_set_ldt.c \
	x86_64_iopl.c x86_64_get_ioperm.c x86_64_set_ioperm.c
.include <bsd.lib.mk>
.else
NOPROG=
.include <bsd.prog.mk>
.endif

