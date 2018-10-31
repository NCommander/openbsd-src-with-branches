#	$OpenBSD: Makefile.inc,v 1.2 2012/09/04 03:10:42 okan Exp $

SRCS+=	bcopy.S memcpy.c memmove.c \
	bcmp.S bzero.S ffs.S strcmp.S strlen.S \
	strchr.S strrchr.S \
	memchr.c memcmp.c memset.c strcat.c strcpy.c strcspn.c \
	strlcat.c strlcpy.c strncat.c strncmp.c strncpy.c \
	strpbrk.c strsep.c strspn.c strstr.c swab.c
