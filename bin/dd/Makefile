#	$OpenBSD: Makefile,v 1.2 1996/06/23 14:19:44 deraadt Exp $
#	$NetBSD: Makefile,v 1.4 1995/03/21 09:03:58 cgd Exp $

PROG=	dd
SRCS=	args.c conv.c conv_tab.c dd.c misc.c position.c

.ifdef (${MACHINE} == "powerpc")
# powerpc compiler has a but that coredumps when compiling code
# with 64 bit math and optimization.
CFLAGS+= -O0
.endif

.include <bsd.prog.mk>
