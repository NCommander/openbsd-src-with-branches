/* $OpenBSD$ */
#include <stdio.h>

extern int g();

int main()
{
	printf("%d\n", g());
	return 0;
}
