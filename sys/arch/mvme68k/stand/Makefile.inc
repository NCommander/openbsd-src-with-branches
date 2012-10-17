#	$OpenBSD: Makefile.inc,v 1.6 2005/12/30 02:03:49 millert Exp $

MDEC_DIR?=/usr/mdec
CFLAGS+=	-fno-stack-protector

# Load addresses for first and second stage bootstraps
STAGE1_RELOC=0x10000 
STAGE2_RELOC=0x6F0000 
