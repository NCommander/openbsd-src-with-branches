#	$OpenBSD: Makefile.inc,v 1.1 2004/04/21 18:40:18 miod Exp $

LDSTATIC=-static

# Hack needed to find kernel images...
BSDSRCDIR?=/usr/src
SRCSYSDIR?=${BSDSRCDIR}/sys
