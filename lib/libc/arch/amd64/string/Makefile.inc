#	$OpenBSD: Makefile.inc,v 1.4 2012/09/04 03:10:42 okan Exp $

SRCS+=  bcmp.S ffs.S index.c memchr.S memcmp.S bcopy.S bzero.S \
        rindex.c strcat.c strcmp.S strcpy.c strcspn.c strlen.c \
        strncat.c strncmp.c strncpy.c strpbrk.c strsep.c \
        strspn.c strstr.c swab.c memset.S strlcpy.c strlcat.c
SRCS+=	memcpy.S memmove.S strchr.S strrchr.S
