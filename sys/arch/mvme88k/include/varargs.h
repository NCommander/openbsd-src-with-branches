/*	$OpenBSD: varargs.h,v 1.3.14.1 2001/10/31 03:01:19 nate Exp $	*/

#ifndef _M88K_VARARGS_H_
#define _M88K_VARARGS_H_

#include <machine/va-m88k.h>

/* Define va_list from __gnuc_va_list.  */
typedef	__gnuc_va_list	va_list;
typedef	_BSD_VA_LIST_	__gnuc_va_list

#endif	/* _M88K_VARARGS_H_ */
