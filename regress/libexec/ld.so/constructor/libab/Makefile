# $OpenBSD$

LIB=ab
SRCS= ab.C
NOPROFILE=yes
CPPFLAGS=-I${.CURDIR}/../libaa
LDADD=-L../libaa
LDADD+=-laa

regress: all

.include <bsd.lib.mk>
