dnl $OpenBSD$
dnl Preserving spaces within nested parentheses
define(`foo',`$1')dnl
foo((	  check for embedded spaces))
