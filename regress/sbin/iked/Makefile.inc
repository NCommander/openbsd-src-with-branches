#	$OpenBSD$

CDIAGFLAGS=	-Wall
#CDIAGFLAGS+=	-Werror
CDIAGFLAGS+=	-Wextra
CDIAGFLAGS+=	-Wpointer-arith
CDIAGFLAGS+=	-Wstrict-prototypes
CDIAGFLAGS+=	-Wmissing-prototypes
CDIAGFLAGS+=	-Wunused
CDIAGFLAGS+=	-Wsign-compare
CDIAGFLAGS+=	-Wshadow
CDIAGFLAGS+=	-Wpointer-sign
CDIAGFLAGS+=	-Wno-unused-parameter #-Wno-error=unused-parameter
CDIAGFLAGS+=	-Wuninitialized
.if (${CC:L} == "gcc" || ${CC:L} == "cc")
CDIAGFLAGS+=	-Wbounded
.endif

DEBUG=	-g
