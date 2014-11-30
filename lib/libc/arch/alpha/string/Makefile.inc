#	$OpenBSD: Makefile.inc,v 1.7 2012/09/04 03:10:42 okan Exp $

SRCS+=	bcopy.S memcpy.c memmove.S \
	index.c rindex.c strchr.c strrchr.c \
	bzero.S ffs.S \
	memchr.c memcmp.c memset.c strcat.c bcmp.c \
	strcmp.c strcpy.c strcspn.c strlen.c strlcat.c strlcpy.c \
	strncat.c strncmp.c strncpy.c strpbrk.c strsep.c \
	strspn.c strstr.c swab.c
