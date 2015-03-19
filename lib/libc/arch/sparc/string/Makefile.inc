#	$OpenBSD: Makefile.inc,v 1.5 2012/09/04 03:10:42 okan Exp $

SRCS+=	bcopy.c memcpy.c memmove.c \
	ffs.S strlen.S bzero.S \
	bcmp.c index.c memchr.c memcmp.c \
	memset.c rindex.c strcat.c strchr.c \
	strcmp.c strcpy.c strcspn.c strlcat.c strlcpy.c \
	strncat.c strncmp.c strncpy.c strpbrk.c strrchr.c strsep.c \
	strspn.c strstr.c swab.c
