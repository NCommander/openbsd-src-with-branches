#	$OpenBSD: cipher-speed.sh,v 1.13 2015/03/24 20:22:17 markus Exp $
#	Placed in the Public Domain.

tid="cipher speed"

getbytes ()
{
	sed -n '/transferred/s/.*secs (\(.* bytes.sec\).*/\1/p'
}

tries="1 2"

for c in `${SSH} -Q cipher`; do n=0; for m in `${SSH} -Q mac`; do
	trace "cipher $c mac $m"
	for x in $tries; do
		printf "$c/$m:\t"
		( ${SSH} -o 'compression no' \
			-F $OBJ/ssh_proxy -m $m -c $c somehost \
			exec sh -c \'"dd of=/dev/null obs=32k"\' \
		< ${DATA} ) 2>&1 | getbytes

		if [ $? -ne 0 ]; then
			fail "ssh failed with mac $m cipher $c"
		fi
	done
	# No point trying all MACs for AEAD ciphers since they are ignored.
	if ${SSH} -Q cipher-auth | grep "^${c}\$" >/dev/null 2>&1 ; then
		break
	fi
	n=$(($n + 1))
done; done

