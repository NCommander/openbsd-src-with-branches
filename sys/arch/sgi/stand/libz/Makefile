#	$OpenBSD: Makefile,v 1.4 2012/08/21 14:46:19 pascal Exp $

S=${.CURDIR}/../../../..
ZDST=${.OBJDIR}

.PATH: ${S}/lib/libz

.include "${S}/lib/libz/Makefile"
CFLAGS+= ${CEXTRAFLAGS} ${SAABI} -mno-abicalls -D_NO_ABICALLS \
	${AFLAGS} -fno-pie -I${S}
