#!/bin/sh
#
# Copyright (C) 2000, 2001  Internet Software Consortium.
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
# DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
# INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
# FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
# NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
# WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

# $ISC: sign.sh,v 1.12 2001/01/09 21:43:02 bwelling Exp $

RANDFILE=../random.data

zone=secure.example.
infile=secure.example.db.in
zonefile=secure.example.db

keyname=`$KEYGEN -r $RANDFILE -a RSA -b 768 -n zone $zone`

$KEYSETTOOL -r $RANDFILE -t 3600 $keyname.key > /dev/null

cat $infile $keyname.key >$zonefile

$SIGNER -r $RANDFILE -o $zone $zonefile > /dev/null

zone=bogus.example.
infile=bogus.example.db.in
zonefile=bogus.example.db

keyname=`$KEYGEN -r $RANDFILE -a RSA -b 768 -n zone $zone`

$KEYSETTOOL -r $RANDFILE -t 3600 $keyname.key > /dev/null

cat $infile $keyname.key >$zonefile

$SIGNER -r $RANDFILE -o $zone $zonefile > /dev/null
