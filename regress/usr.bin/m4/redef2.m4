dnl $OpenBSD$
dnl recursive macro redefinition
define(`A', `aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa')
A(
	define(`A', `bbbbbbbbbbbbbbbbbbb')
)
