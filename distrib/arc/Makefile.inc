#	$OpenBSD: Makefile.inc,v 1.4 1998/03/08 00:26:13 deraadt Exp $

REV=	${OSrev}

LDSTATIC=	-static

# Hack needed to find kernel images...
BSDSRCDIR?=	/usr/src
SRCSYSDIR?=	${BSDSRCDIR}/sys
