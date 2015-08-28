#	$OpenBSD$
.PATH:		${.CURDIR}/..
CFLAGS+=	-I${.CURDIR}/..
CFLAGS+=	-Wall -Wextra -Wshadow -Wno-unused-parameter
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations -Wpointer-arith
.ifdef DEBUG
CFLAGS+=	-DRADIUSD_DEBUG
CFLAGS+=	-Werror
.endif
