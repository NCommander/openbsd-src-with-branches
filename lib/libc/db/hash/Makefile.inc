#	$OpenBSD: Makefile.inc,v 1.6 1996/05/03 21:43:43 cgd Exp $

.PATH: ${.CURDIR}/db/hash

SRCS+=	hash.c hash_bigkey.c hash_buf.c hash_func.c hash_log2.c \
	hash_page.c hsearch.c ndbm.c
