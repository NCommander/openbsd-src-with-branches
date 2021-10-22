/*      $OpenBSD$       */
/* Public domain - Moritz Buhl */

#define __FBSDID(a)
#define rounddown(x, y)	(((x)/(y))*(y))
#define fpequal(a, b)	fpequal_cs(a, b, 1)
#define hexdump(...)
