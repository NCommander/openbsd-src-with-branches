/*	$OpenBSD: encrypt.c,v 1.15 2001/07/31 18:30:38 millert Exp $	*/

/*
 * Copyright (c) 1996, Jason Downs.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <login_cap.h>

/*
 * Very simple little program, for encrypting passwords from the command
 * line.  Useful for scripts and such.
 */

#define DO_MAKEKEY 0
#define DO_DES     1
#define DO_MD5     2
#define DO_BLF     3

extern char *__progname;
char buffer[_PASSWORD_LEN];

void
usage(void)
{

	(void)fprintf(stderr,
	    "usage: %s [-k] [-b rounds] [-m] [-s salt] [-p | string]\n",
	    __progname);
	exit(1);
}

char *
trim(char *line)
{
	char *ptr;

	for (ptr = &line[strlen(line)-1]; ptr > line; ptr--) {
		if (!isspace(*ptr))
			break;
	}
	ptr[1] = '\0';

	for (ptr = line; *ptr && isspace(*ptr); ptr++)
		;

	return(ptr);
}

void
print_passwd(char *string, int operation, void *extra)
{
	char msalt[3], *salt;
	struct passwd pwd;
	login_cap_t *lc;
	int pwd_gensalt(char *, int, struct passwd *, login_cap_t *, char);
	void to64(char *, int32_t, int n);

	switch(operation) {
	case DO_MAKEKEY:
		/*
		 * makekey mode: parse string into separate DES key and salt.
		 */
		if (strlen(string) != 10) {
			/* To be compatible... */
			errx(1, "%s", strerror(EFTYPE));
		}
		strcpy(msalt, &string[8]);
		salt = msalt;
		break;

	case DO_MD5:
		strcpy(buffer, "$1$");
		to64(&buffer[3], arc4random(), 4);
		to64(&buffer[7], arc4random(), 4);
		strcpy(buffer + 11, "$");
		salt = buffer;
		break;

	case DO_BLF:
		strlcpy(buffer, bcrypt_gensalt(*(int *)extra), _PASSWORD_LEN);
		salt = buffer;
		break;

	case DO_DES:
		salt = extra;
		break;

	default:
		pwd.pw_name = "default";
		if ((lc = login_getclass(NULL)) == NULL)
			errx(1, "unable to get default login class.");
		if (!pwd_gensalt(buffer, _PASSWORD_LEN, &pwd, lc, 'l'))
			errx(1, "can't generate salt");
		salt = buffer;
		break;
	}

	(void)fputs(crypt(string, salt), stdout);
}

int
main(int argc, char **argv)
{
	int opt;
	int operation = -1;
	int prompt = 0;
	int rounds;
	void *extra;                       /* Store salt or number of rounds */

	if (strcmp(__progname, "makekey") == 0)
		operation = DO_MAKEKEY;

	while ((opt = getopt(argc, argv, "kmps:b:")) != -1) {
		switch (opt) {
		case 'k':                       /* Stdin/Stdout Unix crypt */
			if (operation != -1 || prompt)
				usage();
			operation = DO_MAKEKEY;
			break;

		case 'm':                       /* MD5 password hash */
			if (operation != -1)
				usage();
			operation = DO_MD5;
			break;

		case 'p':
			if (operation == DO_MAKEKEY)
				usage();
			prompt = 1;
			break;

		case 's':                       /* Unix crypt (DES) */
			if (operation != -1 || optarg[0] == '$')
				usage();
			operation = DO_DES;
			extra = optarg;
			break;

		case 'b':                       /* Blowfish password hash */
			if (operation != -1)
				usage();
			operation = DO_BLF;
			rounds = atoi(optarg);
			extra = &rounds;
			break;

		default:
			usage();
		}
	}

	if (((argc - optind) < 1) || operation == DO_MAKEKEY) {
		char line[BUFSIZ], *string;

		if (prompt) {
			string = getpass("Enter string: ");
			print_passwd(string, operation, extra);
			(void)fputc('\n', stdout);
		} else {
			/* Encrypt stdin to stdout. */
			while (!feof(stdin) &&
			    (fgets(line, sizeof(line), stdin) != NULL)) {
				/* Kill the whitesapce. */
				string = trim(line);
				if (*string == '\0')
					continue;
				
				print_passwd(string, operation, extra);

				if (operation == DO_MAKEKEY) {
					fflush(stdout);
					break;
				}
				(void)fputc('\n', stdout);
			}
		}
	} else {
		char *string;

		/* can't combine -p with a supplied string */
		if (prompt)
			usage();

		/* Perhaps it isn't worth worrying about, but... */
		if ((string = strdup(argv[optind])) == NULL)
			err(1, NULL);
		/* Wipe the argument. */
		memset(argv[optind], 0, strlen(argv[optind]));

		print_passwd(string, operation, extra);

		(void)fputc('\n', stdout);

		/* Wipe our copy, before we free it. */
		memset(string, 0, strlen(string));
		free(string);
	}
	exit(0);
}
