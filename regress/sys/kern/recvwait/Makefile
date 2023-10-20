# $OpenBSD$

# There is a race where a second control message is inserted into
# the receive socket buffer while it is processed by soreceive(9).
# As this code path is rarely triggered in real life, add a
# regress test.

PROG=		mcontrol-stream
WARNINGS=	yes

.include <bsd.regress.mk>
