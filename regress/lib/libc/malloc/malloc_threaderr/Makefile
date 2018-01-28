#	$OpenBSD: Makefile,v 1.3 2017/01/24 16:03:28 otto Exp $

# This test is supposed to print a malloc error and create a core dump

REGRESS_TARGETS=        t1
PROG=			malloc_threaderr
LDADD+=			-pthread
DPADD+=			${LIBPTHREAD}

.include <bsd.regress.mk>

t1:	malloc_threaderr
	${.OBJDIR}/malloc_threaderror 2>&1 | fgrep 'in free(): bogus pointer (double free?)'
