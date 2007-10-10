# $OpenBSD$
# Don't segfault if no file list is given.
#
OBJ=$2
cd ${OBJ}
cpio -o < /dev/null
