/*	$OpenBSD$	*/
/*
 * 'Test' getcwd() and getwd()
 */
#include <stdio.h>
#include <sys/param.h>
#include <unistd.h>
#include "test.h"

int
main(int argc, char **argv)
{
    char wd[MAXPATHLEN];
    char *path;

    ASSERT(path = getcwd(wd, sizeof wd));
    printf("getcwd => %s\n", path);
    ASSERT(path = getwd(wd));
    printf("getwd => %s\n", path);
    SUCCEED;
}
