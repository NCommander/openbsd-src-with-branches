#	$OpenBSD: Makefile,v 1.11 1996/07/16 07:39:16 pefo Exp $
#	$NetBSD: Makefile,v 1.35 1996/04/03 21:20:52 chuck Exp $

SUBDIR+=	bc cpio cvs dc diff diff3 gcc grep groff
SUBDIR+=	gzip rcs sdiff sendbug sort texinfo

# some ports use binutils, some do not.
.if (${MACHINE_ARCH} == "alpha") || (${MACHINE_ARCH} == "mips")
SUBDIR+=	binutils
.else
SUBDIR+=	gas gdb ld
.endif

.include <bsd.subdir.mk>
