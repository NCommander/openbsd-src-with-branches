#	$OpenBSD: Makefile.inc,v 1.2 1997/09/21 11:45:48 deraadt Exp $

REV=	22

LDSTATIC=	-static

# Hack needed to find kernel images...
BSDSRCDIR?=	/usr/src
SRCSYSDIR?=	${BSDSRCDIR}/sys
