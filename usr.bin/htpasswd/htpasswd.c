/*	$OpenBSD$ */
/*
 * Copyright (c) 2014 Florian Obser <florian@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <readpassphrase.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

__dead void	usage(void);
void		nag(char*);

extern char *__progname;

__dead void
usage(void)
{
	fprintf(stderr, "usage:\t%s [file] login\n", __progname);
	exit(1);
}

int
main(int argc, char** argv)
{
	FILE *in, *out;
	size_t linesize;
	ssize_t linelen;
	int fd, loginlen;
	char hash[_PASSWORD_LEN], *file, *line, *login, pass[1024], pass2[1024];
	char salt[_PASSWORD_LEN], tmpl[sizeof("/tmp/htpasswd-XXXXXXXXXX")];

	file = NULL;
	login = NULL;
	in = NULL;
	out = NULL;
	line = NULL;
	linesize = 0;

	switch (argc) {
	case 2:
		if ((loginlen = asprintf(&login, "%s:", argv[1])) == -1)
			err(1, "asprintf");
		break;
	case 3:
		file = argv[1];
		if ((loginlen = asprintf(&login, "%s:", argv[2])) == -1)
			err(1, "asprintf");
		break;
	default:
		usage();
		/* NOT REACHED */
		break;
	}

	if (!readpassphrase("Password: ", pass, sizeof(pass), RPP_ECHO_OFF))
		err(1, "unable to read password");
	if (!readpassphrase("Retype Password: ", pass2, sizeof(pass2),
	    RPP_ECHO_OFF)) {
		explicit_bzero(pass, sizeof(pass));
		err(1, "unable to read password");
	}
	if (strcmp(pass, pass2) != 0) {
		explicit_bzero(pass, sizeof(pass));
		explicit_bzero(pass2, sizeof(pass2));
		errx(1, "passwords don't match");
	}

	explicit_bzero(pass2, sizeof(pass2));
	if (strlcpy(salt, bcrypt_gensalt(8), sizeof(salt)) >= sizeof(salt))
		err(1, "salt too long");
	if (strlcpy(hash, bcrypt(pass, salt), sizeof(hash)) >= sizeof(hash))
		err(1, "hash too long");
	explicit_bzero(pass, sizeof(pass));

	if (file == NULL)
		printf("%s%s\n", login, hash);
	else {
		if ((in = fopen(file, "r+")) == NULL) {
			if (errno == ENOENT) {
				if ((out = fopen(file, "w")) == NULL)
					err(1, "cannot open password file for"
					    " reading or writing");
				if (fchmod(fileno(out), S_IRUSR | S_IWUSR)
				    == -1)
					err(1, "cannot chmod new password"
					    " file");
			} else
				err(1, "cannot open password file for reading");
		}
		/* file already exits, copy content and filter login out */
		if (out == NULL) {
			strlcpy(tmpl, "/tmp/htpasswd-XXXXXXXXXX", sizeof(tmpl));
			if ((fd = mkstemp(tmpl)) == -1)
				err(1, "mkstemp");

			if ((out = fdopen(fd, "w+")) == NULL)
				err(1, "cannot open tempfile");

			while ((linelen = getline(&line, &linesize, in))
			    != -1) {
				if (strncmp(line, login, loginlen) != 0) {
					if (fprintf(out, "%s", line) == -1)
						err(1, "cannot write to temp "
						    "file");
					nag(line);
				}
			}
		}
		if (fprintf(out, "%s%s\n", login, hash) == -1)
			err(1, "cannot write new password hash");

		/* file already exists, overwrite it */
		if (in != NULL) {
			if (fseek(in, 0, SEEK_SET) == -1)
				err(1, "cannot seek in password file");
			if (ftruncate(fileno(in), 0) == -1)
				err(1, "cannot truncate password file");
			if (fseek(out, 0, SEEK_SET) == -1)
				err(1, "cannot seek in temp file");
			while ((linelen = getline(&line, &linesize, out))
			    != -1)
				if (fprintf(in, "%s", line) == -1)
					err(1, "cannot write to password file");
			if (fclose(in) == EOF)
				err(1, "cannot close password file");
		}
		if (fclose(out) == EOF) {
			if (in != NULL)
				err(1, "cannot close temp file");
			else
				err(1, "cannot close password file");
		}
		if (in != NULL && unlink(tmpl) == -1)
			err(1, "cannot delete temp file (%s)", tmpl);
	}
	exit(0);
}

void
nag(char* line)
{
	char *tok;
	if (strtok(line, ":") != NULL)
		if ((tok = strtok(NULL, ":")) != NULL)
			if (strncmp(tok, "$2a$", 4) != 0 &&
			     strncmp(tok, "$2b$", 4) != 0)
				fprintf(stderr, "%s doesn't use bcrypt."
				    " Update the password.\n", line);
}
