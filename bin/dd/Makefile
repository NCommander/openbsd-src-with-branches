#	$OpenBSD: Makefile,v 1.3 1997/05/21 23:02:46 rahnds Exp $

PROG=	dd
SRCS=	args.c conv.c conv_tab.c dd.c misc.c position.c

.ifdef (${MACHINE} == "powerpc")
# powerpc compiler has a but that coredumps when compiling code
# with 64 bit math and optimization.
CFLAGS+= -O0
.endif

.include <bsd.prog.mk>
