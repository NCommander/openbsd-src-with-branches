# $OpenBSD$

PROG = sqlite3
SRCS = shell.c

.include <bsd.own.mk>
.if defined(NOPIC)
CPPFLAGS +=	-DSQLITE_OMIT_LOAD_EXTENSION=1
.endif


CPPFLAGS +=	-DHAVE_READLINE=1

CFLAGS += -pthread
LDFLAGS += -pthread
LDADD += -lsqlite3 -lreadline -lcurses

.include <bsd.prog.mk>
