/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD$";
#endif /* LIBC_SCCS and not lint */

#include <sys/localedef.h>
#include <locale.h>

const _NumericLocale _DefaultNumericLocale = 
{
	".",
	"",
	""
};

const _NumericLocale *_CurrentNumericLocale = &_DefaultNumericLocale;
