#	$OpenBSD: Makefile,v 1.2 2004/09/16 18:54:49 pefo Exp $

S=${.CURDIR}/../../../..
ZDST=${.OBJDIR}

.PATH: ${S}/lib/libz

.include "${S}/lib/libz/Makefile"
CFLAGS+= ${CEXTRAFLAGS} ${SAABI} -mno-abicalls -D_NO_ABICALLS \
	${AFLAGS} -I${S}
