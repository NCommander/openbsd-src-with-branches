#	$OpenBSD$

# To test /usr/src/gnu/lib/libreadline, run "make READLINE=1".
# To test /usr/src/lib/libedit/readline, run just "make".

PROG =		history

.ifdef READLINE
CFLAGS +=	-DREADLINE
LDADD +=	-lcurses -lreadline
.else
LDADD +=	-lcurses -ledit
.endif

.include <bsd.regress.mk>
