#! /bin/sh
# $OpenBSD$

# set -e is supposed to abort the script for errors that are not caught
# otherwise.

set -e

for i in 1 2 3
do
	false && true
done
true
