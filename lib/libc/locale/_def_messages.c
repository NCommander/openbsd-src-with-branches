/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD$";
#endif /* LIBC_SCCS and not lint */

#include <sys/localedef.h>
#include <locale.h>

const _MessagesLocale _DefaultMessagesLocale = 
{
	"^[Yn]",
	"^[Nn]",
	"yes",
	"no"
} ;

const _MessagesLocale *_CurrentMessagesLocale = &_DefaultMessagesLocale;
