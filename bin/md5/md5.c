/*
 * $OpenBSD: md5.c,v 1.5 1997/07/12 21:09:02 millert Exp $
 *
 * Derived from:
 *	MDDRIVER.C - test driver for MD2, MD4 and MD5
 */

/*
 *  Copyright (C) 1990-2, RSA Data Security, Inc. Created 1990. All
 *  rights reserved.
 *
 *  RSA Data Security, Inc. makes no representations concerning either
 *  the merchantability of this software or the suitability of this
 *  software for any particular purpose. It is provided "as is"
 *  without express or implied warranty of any kind.
 *
 *  These notices must be retained in any copies of any part of this
 *  documentation and/or software.
 */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include <md5.h>
#include <sha1.h>
#include <rmd160.h>

/*
 * Length of test block, number of test blocks.
 */
#define TEST_BLOCK_LEN 10000
#define TEST_BLOCK_COUNT 10000

extern char *__progname;

static void MDString __P((char *));
static void MDTimeTrial __P((void *));
static void MDTestSuite __P((void));
static void MDFilter __P((int, void *));

int main __P((int, char *[]));

/*
 * Globals for indirection...
 */
void (*MDInit)();
void (*MDUpdate)();
char * (*MDEnd)();
char * (*MDFile)();
char * (*MDData)();
char *MDType;

/* Main driver.
 *
 * Arguments (may be any combination):
 *   -sstring - digests string
 *   -t       - runs time trial
 *   -x       - runs test script
 *   filename - digests file
 *   (none)   - digests standard input
 */
int
main(argc, argv)
	int     argc;
	char   *argv[];
{
	int     i;
	char   *p;
	char	buf[41];
	void   *context;

	/* What were we called as?  Default to md5 */
	if (strcmp(__progname, "rmd160") == 0) {
		MDType = "RMD160";
		MDInit = RMD160Init;
		MDUpdate = RMD160Update;
		MDEnd = RMD160End;
		MDFile = RMD160File;
		MDData = RMD160Data;
		if ((context = malloc(sizeof(RMD160_CTX))) == NULL)
			err(1, "malloc");
	} else if (strcmp(__progname, "sha1") == 0) {
		MDType = "SHA1";
		MDInit = SHA1Init;
		MDUpdate = SHA1Update;
		MDEnd = SHA1End;
		MDFile = SHA1File;
		MDData = SHA1Data;
		if ((context = malloc(sizeof(SHA1_CTX))) == NULL)
			err(1, "malloc");
	} else {
		MDType = "MD5";
		MDInit = MD5Init;
		MDUpdate = MD5Update;
		MDEnd = MD5End;
		MDFile = MD5File;
		MDData = MD5Data;
		if ((context = malloc(sizeof(MD5_CTX))) == NULL)
			err(1, "malloc");
	}

	if (argc > 1)
		for (i = 1; i < argc; i++)
			if (argv[i][0] == '-' && argv[i][1] == 's')
				MDString(argv[i] + 2);
			else if (strcmp(argv[i], "-t") == 0)
				MDTimeTrial(context);
			else if (strcmp(argv[i], "-p") == 0)
				MDFilter(1, context);
			else if (strcmp(argv[i], "-x") == 0)
				MDTestSuite();
			else {
				p = MDFile(argv[i], buf);
				if (!p)
					warnx(argv[i]);
				else
					(void)printf("%s (%s) = %s\n", MDType,
					    argv[i], p);
			}
	else
		MDFilter(0, context);

	exit(0);
}
/*
 * Digests a string and prints the result.
 */
static void
MDString(string)
	char   *string;
{
	size_t len = strlen(string);
	char buf[41];

	(void)printf("%s (\"%s\") = %s\n", MDType, string,
	    MDData(string, len, buf));
}
/*
 * Measures the time to digest TEST_BLOCK_COUNT TEST_BLOCK_LEN-byte blocks.
 */
static void
MDTimeTrial(context)
	void *context;
{
	time_t  endTime, startTime;
	unsigned char block[TEST_BLOCK_LEN];
	unsigned int i;
	char   *p, buf[41];

	(void)printf("%s time trial. Digesting %d %d-byte blocks ...", MDType,
	    TEST_BLOCK_LEN, TEST_BLOCK_COUNT);
	fflush(stdout);

	/* Initialize block */
	for (i = 0; i < TEST_BLOCK_LEN; i++)
		block[i] = (unsigned char) (i & 0xff);

	/* Start timer */
	time(&startTime);

	/* Digest blocks */
	MDInit(context);
	for (i = 0; i < TEST_BLOCK_COUNT; i++)
		MDUpdate(context, block, (size_t)TEST_BLOCK_LEN);
	p = MDEnd(context,buf);

	/* Stop timer */
	time(&endTime);

	(void)printf(" done\nDigest = %s", p);
	(void)printf("\nTime = %ld seconds\n", (long) (endTime - startTime));
	/*
	 * Be careful that endTime-startTime is not zero.
	 * (Bug fix from Ric Anderson <ric@Artisoft.COM>)
	 */
	(void)printf("Speed = %ld bytes/second\n",
	    (long) TEST_BLOCK_LEN * (long) TEST_BLOCK_COUNT /
	    ((endTime - startTime) != 0 ? (endTime - startTime) : 1));
}
/*
 * Digests a reference suite of strings and prints the results.
 */
static void
MDTestSuite()
{
	(void)printf("%s test suite:\n", MDType);

	MDString("");
	MDString("a");
	MDString("abc");
	MDString("message digest");
	MDString("abcdefghijklmnopqrstuvwxyz");
	MDString
	    ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
	MDString
	    ("1234567890123456789012345678901234567890\
1234567890123456789012345678901234567890");
	MDString("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq");
}

/*
 * Digests the standard input and prints the result.
 */
static void
MDFilter(pipe, context)
	int pipe;
	void *context;
{
	size_t	len;
	unsigned char buffer[BUFSIZ];
	char buf[41];

	MDInit(context);
	while ((len = fread(buffer, (size_t)1, (size_t)BUFSIZ, stdin)) > 0) {
		if (pipe && (len != fwrite(buffer, (size_t)1, len, stdout)))
			err(1, "stdout");
		MDUpdate(context, buffer, len);
	}
	(void)printf("%s\n", MDEnd(context,buf));
}
