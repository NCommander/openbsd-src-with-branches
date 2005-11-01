#  Copyright (c) 1999 Mats O Jansson, Stockholm, Sweden
#  All rights reserved.
#
# @(#) $OpenBSD$

INSTBIN=	install -c -s -o root -g root -m 755
INSTINIT=	install -c -o root -g root -m 755

install:	mopd.init
	echo "Installing files in linux2";
	$(INSTINIT) $< $(INSTALL)/etc/rc.d/init.d/mopd

