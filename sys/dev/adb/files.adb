#	$OpenBSD$

file	dev/adb/adb_subr.c		adb

device	akbd: wskbddev
attach	akbd at adb
file	dev/adb/akbd.c			akbd needs-flag

device	ams: wsmousedev
attach	ams at adb
file	dev/adb/ams.c			ams needs-flag
