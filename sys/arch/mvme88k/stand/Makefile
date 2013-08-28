#	$OpenBSD: Makefile,v 1.7 2004/01/28 08:46:29 miod Exp $

.if ${MACHINE} == "mvme88k"
SUBDIR=  bugcrt libbug libsa libz
SUBDIR+= bootxx bootsd wrtvid bootst
SUBDIR+= netboot sboot tftpboot
.endif

SUBDIR+=installboot

.include <bsd.subdir.mk>
