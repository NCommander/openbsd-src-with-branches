#	$OpenBSD: Makefile.inc,v 1.5 1995/02/27 13:23:12 cgd Exp $

.PATH: ${.CURDIR}/db/man

# mpool.3
MAN+=	btree.3 dbopen.3 hash.3 recno.3
MLINKS+= dbopen.3 db.3
