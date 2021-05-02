/*	$OpenBSD$	*/

/*
 * Written by Martynas Venckus.  Public domain
 */

#define	type		long double
#define	roundit		roundl
#define	dtype		long long
#define	DTYPE_MIN	LLONG_MIN
#define	DTYPE_MAX	LLONG_MAX
#define	fn		llroundl

#include "s_lroundl.c"
