dnl $OpenBSD$
dnl Another test, this time for multiple wrappers
dnl Check the behavior in presence of recursive m4wraps
dnl both for POSIX m4 and for gnu-m4 mode
m4wrap(`this is
')dnl
m4wrap(`a string
')dnl
m4wrap(`m4wrap(`recurse
')')dnl
normal m4 stuff
