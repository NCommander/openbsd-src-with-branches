/*
 * Public Domain 2003 Dale Rahn
 *
 * $OpenBSD: main.c,v 1.2 2005/09/18 19:58:50 drahn Exp $
 */
#include <stdio.h>
#include <dlfcn.h>


void ad(void);
extern int libglobal;

void (*ad_f)(void) = &ad;
int *a = &libglobal;
int
main()
{

	ad_f();

	return 1;
}
