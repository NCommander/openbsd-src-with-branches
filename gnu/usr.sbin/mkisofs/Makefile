#	$OpenBSD$

PROG=	mkisofs
MAN=	mkisofs.8

SRCS=	mkisofs.c tree.c write.c hash.c rock.c exclude.c multi.c \
	files.c fnmatch.c match.c name.c eltorito.c

CFLAGS+=-DSYSTEM_ID_DEFAULT=\"OpenBSD\" -I${.CURDIR}

.include <bsd.prog.mk>
