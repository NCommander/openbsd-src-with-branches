#!/bin/sh
# $OpenBSD: skeyinfo.sh,v 1.2 1996/06/26 05:39:22 deraadt Exp $
# search /etc/skeykeys for the skey string for
# this user OR user specified in 1st parameter

if [ -z "$1" ]; then
	WHO=`/usr/bin/whoami`
else
	WHO=$1
fi

if [ -f /etc/skeykeys ]; then
	/usr/bin/awk "{ if (\$1 == \"$WHO\" && \$2 ~ /^MD[0-9]+/) {print \$3-1,\$4} else {print \$2-1,\$3} }" < /etc/skeykeys
fi
