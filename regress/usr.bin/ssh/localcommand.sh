#	$OpenBSD: try-ciphers.sh,v 1.11 2007/06/07 19:41:46 pvalchev Exp $
#	Placed in the Public Domain.

tid="localcommand"

echo 'PermitLocalCommand yes' >> $OBJ/ssh_proxy
echo 'LocalCommand echo foo' >> $OBJ/ssh_proxy

for p in 1 2; do
	verbose "test $tid: proto $p localcommand"
	a=$(${SSH} -F $OBJ/ssh_proxy -$p somehost true)
	if [ "$a" != "foo" ] ; then
		fail "$tid proto $p"
	fi
done
