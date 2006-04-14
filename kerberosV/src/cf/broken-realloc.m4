dnl
dnl $KTH: broken-realloc.m4,v 1.4 2005/04/27 12:09:45 lha Exp $
dnl
dnl Test for realloc that doesn't handle NULL as first parameter
dnl
AC_DEFUN([rk_BROKEN_REALLOC], [
AC_CACHE_CHECK(if realloc if broken, ac_cv_func_realloc_broken, [
ac_cv_func_realloc_broken=no
AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <stddef.h>
#include <stdlib.h>

int main()
{
	return realloc(NULL, 17) == NULL;
}
]])],[:], [ac_cv_func_realloc_broken=yes],[:])
])
if test "$ac_cv_func_realloc_broken" = yes ; then
	AC_DEFINE(BROKEN_REALLOC, 1, [Define if realloc(NULL) doesn't work.])
fi
AH_BOTTOM([#ifdef BROKEN_REALLOC
#define realloc(X, Y) rk_realloc((X), (Y))
#endif])
])
