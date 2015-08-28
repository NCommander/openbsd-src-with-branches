#	$OpenBSD: Makefile,v 1.44 2013/10/15 13:28:04 miod Exp $

SUBDIR=	special notes

.if	make(obj)
SUBDIR+=alpha amd64 armish armv7 aviion hppa i386 \
	landisk loongson luna88k macppc \
	octeon sgi socppc sparc sparc64 vax zaurus
.elif exists(${MACHINE})
SUBDIR+=${MACHINE}
.endif

.include <bsd.subdir.mk>
