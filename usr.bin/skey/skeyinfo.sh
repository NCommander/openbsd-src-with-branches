#!/bin/sh
# $Id: skeyinfo.sh,v 1.1 1994/06/24 08:06:46 deraadt Exp $
# search /etc/skeykeys for the skey string for
# this user OR user specified in 1st parameter

if [ -z "$1" ]
then
	WHO=`/usr/bin/whoami`
else
	WHO=$1
fi
if [ -f /etc/skeykeys ]
then
	SKEYINFO=`/usr/bin/grep "^$WHO[ 	]" /etc/skeykeys`
else
	SKEYINFO=`cat /etc/skeykeys|/usr/bin/grep "^$WHO[ 	]"`
fi
echo $SKEYINFO|/usr/bin/awk '{print $2-1,$3}'
