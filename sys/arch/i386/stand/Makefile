#	$OpenBSD: Makefile,v 1.7 1997/07/28 22:03:25 mickey Exp $

SUBDIR=	etc libsa libz biosboot installboot boot mbr

test-fd0:
	sudo mount /dev/fd0a /mnt
	sudo cp /usr/mdec/boot /mnt
	sudo /usr/mdec/installboot -v /mnt/boot /usr/mdec/biosboot fd0
	sudo umount /mnt

.include <bsd.subdir.mk>
