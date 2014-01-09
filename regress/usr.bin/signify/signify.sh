#!/bin/sh
#
# $OpenBSD: basename.sh,v 1.2 2005/04/12 06:45:03 otto Exp $

srcdir=$1

pubkey="$srcdir/regresskey.pub"
seckey="$srcdir/regresskey.sec"
orders="$srcdir/orders.txt"
forgery="$srcdir/forgery.txt"

set -e

signify -p $pubkey -V $orders > /dev/null
signify -p $pubkey -V $forgery 2> /dev/null && exit 1

true
