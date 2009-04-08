#	$OpenBSD: Makefile,v 1.35 2008/08/22 15:52:33 deraadt Exp $

SUBDIR=	special notes

.if	make(obj)
SUBDIR+=alpha amd64 armish hp300 hppa i386 landisk mac68k macppc \
	mvme68k mvme88k mvmeppc sgi socppc sparc sparc64 vax zaurus
.elif exists(${MACHINE})
SUBDIR+=${MACHINE}
.endif

.include <bsd.subdir.mk>
