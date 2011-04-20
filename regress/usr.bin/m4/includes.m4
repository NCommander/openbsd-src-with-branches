dnl $OpenBSD$
dnl Check that include can occur within parameters
define(`foo', include(includes.aux))dnl
foo
