#	$OpenBSD: Makefile.inc,v 1.1 2008/05/15 20:18:00 kettenis Exp $

.if exists(${STANDIR}/mkboot/mkboot)
MKBOOT=${STANDIR}/mkboot/mkboot
.else
MKBOOT=${STANDIR}/mkboot/${__objdir}/mkboot
.endif

BINDIR=/usr/mdec
