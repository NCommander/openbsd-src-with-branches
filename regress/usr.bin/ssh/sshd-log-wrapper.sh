#!/bin/sh
#       $OpenBSD$
#       Placed in the Public Domain.
#
# simple wrapper for sshd proxy mode to catch stderr output
# sh sshd-log-wrapper.sh /path/to/sshd /path/to/logfile

sshd=$1
log=$2
shift
shift

exec $sshd $@ -e 2>$log
