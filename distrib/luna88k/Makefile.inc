#	$OpenBSD$

REV=${OSrev}

LDSTATIC=-static

# Hack needed to find kernel images...
BSDSRCDIR?=/usr/src
SRCSYSDIR?=${BSDSRCDIR}/sys
