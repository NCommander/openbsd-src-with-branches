#	$OpenBSD$

MDEC_DIR?=/usr/mdec
CFLAGS+=	-fno-stack-protector

# Load addresses for first and second stage bootstraps 
STAGE1_RELOC=4000
STAGE2_RELOC=6000
STAGE3_RELOC=6000


