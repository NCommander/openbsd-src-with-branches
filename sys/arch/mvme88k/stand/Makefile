#	$OpenBSD: Makefile,v 1.5 2000/12/28 21:21:25 smurph Exp $
.if ${MACHINE} == "mvme88k"

SUBDIR=  bugcrt libbug libsa libz wrtvid bootsd bootxx bootst netboot
# not ready yet
# SUBDIR+=	tftpboot
# not really written yet...
# SUBDIR+=	sboot
.endif

SUBDIR+=installboot

.include <bsd.subdir.mk>
