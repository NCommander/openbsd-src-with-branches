#	$OpenBSD: Makefile.inc,v 1.1 1997/02/16 18:19:57 downsj Exp $

# Revision is 2.0
REV=	20

LDSTATIC=	-static

# Hack needed to find kernel images...
BSDSRCDIR?=	/usr/src
SRCSYSDIR?=	${BSDSRCDIR}/sys
