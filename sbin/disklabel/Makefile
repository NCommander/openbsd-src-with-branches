#	$OpenBSD: Makefile,v 1.68 2020/07/21 13:56:08 deraadt Exp $

PROG=	disklabel
SRCS=	disklabel.c dkcksum.c editor.c manual.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN=	disklabel.8 disklabel.5

CLEANFILES += disklabel.cat8 manual.c

.include <bsd.own.mk>

.ifdef NOMAN
manual.c:
	(echo 'const unsigned char manpage[] = {'; \
	echo 'no manual' | gzip -9c | hexdump -ve '"0x" 1/1 "%02x,"'; \
	echo '};'; echo 'const int manpage_sz = sizeof(manpage);') > manual.c
.else
disklabel.cat8:	disklabel.8
	mandoc -Tascii ${.ALLSRC} > ${.TARGET}

manual.c:	disklabel.cat8
	(echo 'const unsigned char manpage[] = {'; \
	cat disklabel.cat8 | gzip -9c | hexdump -ve '"0x" 1/1 "%02x,"'; \
	echo '};'; echo 'const int manpage_sz = sizeof(manpage);') > manual.c
.endif

.include <bsd.prog.mk>
