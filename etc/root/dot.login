tset -Q \?$TERM

if ( `logname` == `whoami` ) then
	echo "Don't login as root, use su"
endif

echo 'If you are new to OpenBSD, type "man afterboot".'
