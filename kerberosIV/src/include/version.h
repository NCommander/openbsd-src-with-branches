/*	$OpenBSD$	*/

#include <config.h>

/* Use PACKAGE and VERSION in config.h to build a suitable version string */
#define KRB4_VERSION PACKAGE "-" VERSION

char *krb4_long_version = "@(#)$Version: " KRB4_VERSION " $";
char *krb4_version = KRB4_VERSION;
