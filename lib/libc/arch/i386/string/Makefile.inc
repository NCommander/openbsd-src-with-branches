#	$OpenBSD: Makefile.inc,v 1.6 2012/09/04 03:10:42 okan Exp $

SRCS+=	bcopy.S memcpy.c memmove.S \
	strchr.S strrchr.S \
	bcmp.S bcopy.S bzero.S ffs.S memchr.S memcmp.S swab.S \
	memset.S strcat.S strcmp.S strcpy.S \
	strcspn.c strlen.c strlcat.c strlcpy.c \
	strncat.c strncmp.S strncpy.c strpbrk.c strsep.c \
	strspn.c strstr.c
