/* $OpenBSD: keynote-sign.c,v 1.7 1999/10/09 06:59:37 angelos Exp $ */
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

#if HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#if STDC_HEADERS
#include <string.h>
#endif /* STDC_HEADERS */

#if HAVE_FCNTL_H
#include <fcntl.h>
#endif /* HAVE_FCNTL_H */

#if HAVE_IO_H
#include <io.h>
#elif HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_IO_H */

#include "header.h"
#include "keynote.h"

void
signusage(void)
{
    fprintf(stderr, "Arguments:\n");
    fprintf(stderr, "\t[-v] <AlgorithmName> <AssertionFile> "
	    "<PrivateKeyFile>\n");
}

void
keynote_sign(int argc, char *argv[])
{
    int begin = SIG_PRINT_OFFSET, prlen = SIG_PRINT_LENGTH;
    char *buf, *buf2, *sig, *algname;
    int fd, flg = 0, buflen;
    struct stat sb;

    if ((argc != 4) &&
	(argc != 5))
    {
	signusage();
	exit(-1);
    }

    if (argc == 5)
    {
	if (!strcmp("-v", argv[1]))
	  flg = 1;
	else
	{
	    fprintf(stderr,
		    "Invalid first argument [%s] or too many arguments\n",
		    argv[1]);
	    exit(-1);
	}
    }

    /* Fix algorithm name */
    if (argv[1 + flg][strlen(argv[1 + flg]) - 1] != ':')
    {
        fprintf(stderr, "Algorithm name [%s] should be terminated with a "
		"colon, fixing.\n", argv[1 + flg]);
	algname = (char *) calloc(strlen(argv[1 + flg]) + 2, sizeof(char));
	if (algname == (char *) NULL)
	{
	    perror("calloc()");
	    exit(-1);
	}

	strcpy(algname, argv[1 + flg]);
	algname[strlen(algname)] = ':';
    }
    else
	algname = argv[1 + flg];

    /* Read assertion */
    fd = open(argv[2 + flg], O_RDONLY, 0);
    if (fd < 0)
    {
	perror(argv[2 + flg]);
	exit(-1);
    }

    if (fstat(fd, &sb) < 0)
    {
	perror("fstat()");
	exit(-1);
    }

    if (sb.st_size == 0) /* Paranoid */
    {
	fprintf(stderr, "Error: zero-sized assertion-file.\n");
	exit(-1);
    }

    buflen = sb.st_size + 1;
    buf = (char *) calloc(buflen, sizeof(char));
    if (buf == (char *) NULL)
    {
	perror("calloc()");
	exit(-1);
    }

    if (read(fd, buf, buflen - 1) < 0)
    {
	perror("read()");
	exit(-1);
    }

    close(fd);

    /* Read private key file */
    fd = open(argv[3 + flg], O_RDONLY, 0);
    if (fd < 0)
    {
	perror(argv[3 + flg]);
	exit(-1);
    }

    if (fstat(fd, &sb) < 0)
    {
	perror("fstat()");
	exit(-1);
    }

    if (sb.st_size == 0) /* Paranoid */
    {
	fprintf(stderr, "Illegal key-file size 0\n");
	exit(-1);
    }

    buf2 = (char *) calloc(sb.st_size + 1, sizeof(char));
    if (buf2 == (char *) NULL)
    {
	perror("calloc()");
	exit(-1);
    }

    if (read(fd, buf2, sb.st_size) < 0)
    {
	perror("read()");
	exit(-1);
    }

    close(fd);

    sig = kn_sign_assertion(buf, buflen, buf2, algname, flg);

    /* Free buffers */
    free(buf);
    free(buf2);

    if (sig == (char *) NULL)
    {
	switch (keynote_errno)
	{
	    case ERROR_MEMORY:
		fprintf(stderr, "Out of memory while creating signature.\n");
		break;

	    case ERROR_SYNTAX:
		fprintf(stderr, "Bad assertion or algorithm format, or "
			"unsupported algorithm while creating signature.\n");
		break;

	    default:
		fprintf(stderr, "Unknown error while creating signature.\n");
	}

	exit(-1);
    }

    /* Print signature string */
    print_key(stdout, "", sig, begin, prlen);

    free(sig);   /* Just a reminder that the result is malloc'ed */

    exit(0);
}
