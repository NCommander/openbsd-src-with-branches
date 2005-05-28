# $OpenBSD$

PROG	= gzsig
SRCS	= gzsig.c key.c sign.c ssh2.c util.c verify.c x509.c

LDADD	= -lcrypto -lm

CLEANFILES	+= TAGS *~

.include <bsd.prog.mk>
