#	$OpenBSD: localcommand.sh,v 1.3 2015/03/03 22:35:19 markus Exp $
#	Placed in the Public Domain.

tid="localcommand"

echo 'PermitLocalCommand yes' >> $OBJ/ssh_proxy
echo 'LocalCommand echo foo' >> $OBJ/ssh_proxy

verbose "test $tid: proto $p localcommand"
a=`${SSH} -F $OBJ/ssh_proxy somehost true`
if [ "$a" != "foo" ] ; then
	fail "$tid proto $p"
fi
