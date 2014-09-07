#	$OpenBSD: Makefile,v 1.7 2012/08/21 14:46:20 pascal Exp $

S=${.CURDIR}/../../../..
ZDST=${.OBJDIR}

.PATH: ${S}/lib/libz

.include "${S}/lib/libz/Makefile"
CFLAGS+=${CEXTRAFLAGS} ${AFLAGS} -I${S} -fno-pie
