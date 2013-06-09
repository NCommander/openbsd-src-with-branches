#!/bin/sh
# $OpenBSD$

set -e
true && true && false
echo "should not print"
