#	$OpenBSD: Makefile,v 1.7 1997/09/21 09:29:24 niklas Exp $

.if	make(obj)
SUBDIR= special alpha amiga arc crunch hp300 i386 mac68k mvme68k \
	pmax powerpc sparc
.elif	!make(install)
SUBDIR= special
.endif
.if exists(${MACHINE})
SUBDIR+= ${MACHINE}
.endif
SUBDIR+= notes

.include <bsd.subdir.mk>
