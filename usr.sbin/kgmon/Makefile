#	$OpenBSD$

PROG=	kgmon
MAN=	kgmon.8
DPADD=	${LIBKVM}
LDADD=	-lkvm
#
# This program may safely be run setuid-root to allow non-root
# users to start, stop, and reset profiling buffers.
#
#BINOWN=root
#BINMODE=4555

.include <bsd.prog.mk>
