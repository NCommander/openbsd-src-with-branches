#	$OpenBSD$

CFLAGS += -fpic
AFLAGS += -fpic
#ELF_LDFLAGS=-t
LDADD=	`$(CC) -print-libgcc-file-name`
