#!/bin/sh
#	$OpenBSD$	

run() {
    for i in $* ; do
	./parsetest $(echo "ibase=16; $i" | bc)
	if [ $? -ne 0 ]
	then
	    printf "\nFailed on 0x%s\n" "$i"
	    return 1
	fi
	printf " $i"
    done
    printf "\n"
    return 0
}

case $1 in
    hex)
	printf "Unable to handle %%x format names - DISABLED\n"
	# run $2
	exit 0 ;;
    dec)
	printf "Testing %%d & %%u format names"
	run $2
	exit $? ;;
    static)
	printf "Testing staticly named usage names"
	run $2
	exit $? ;;
esac

