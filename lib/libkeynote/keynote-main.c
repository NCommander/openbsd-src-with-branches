/*
 * The author of this code is Angelos D. Keromytis (angelos@dsl.cis.upenn.edu)
 *
 * This code was written by Angelos D. Keromytis in Philadelphia, PA, USA,
 * in April-May 1998
 *
 * Copyright (C) 1998, 1999 by Angelos D. Keromytis.
 *	
 * Permission to use, copy, and modify this software without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software. 
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, THE AUTHORS MAKES NO
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>

#ifdef WIN32
#include <ctype.h>
#include <io.h>
#else
#include <unistd.h>
#endif /* WIN32 */

extern int keynote_sign(int, char **), keynote_sigver(int, char **);
extern int keynote_verify(int, char **), keynote_keygen(int, char **);

#ifdef WIN32
void
#else
int
#endif
main(int argc, char *argv[])
{
    if (argc < 2)
    {
	fprintf(stderr, "Insufficient number of arguments.\n");
	exit(-1);
    }

    if (!strcmp(argv[1], "sign"))
      keynote_sign(argc - 1, argv + 1);
    else
      if (!strcmp(argv[1], "verify"))
	keynote_verify(argc - 1, argv + 1);
      else
	if (!strcmp(argv[1], "sigver"))
	  keynote_sigver(argc - 1, argv + 1);
	else
	  if (!strcmp(argv[1], "keygen"))
	    keynote_keygen(argc - 1, argv + 1);

    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "\tsign ...\n");
    fprintf(stderr, "\tsigver ...\n");
    fprintf(stderr, "\tverify ...\n");
    fprintf(stderr, "\tkeygen ...\n");
    exit(-1);
}
