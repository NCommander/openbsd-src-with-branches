#	$OpenBSD: Makefile,v 1.5 2000/03/03 00:54:52 todd Exp $

.if ${MACHINE} == "mvme68k"
SUBDIR= bugcrt libbug libsa libz sboot netboot bootxx bootsd bootst \
	prtvid wrtvid
.endif

SUBDIR+=installboot

.include <bsd.subdir.mk>
