/*	$OpenBSD$	*/
/*
 * Copyright 2000 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <regex.h>
#include <grp.h>
#include <paths.h>
#include <pwd.h>
#include <sys/errno.h>

struct pattern {
	char *match;
	int flags;
	char *response;
};

struct pattern patterns[] = {
	{
		"^[0-9]*$", 
		REG_EXTENDED|REG_NOSUB,
		"Please don't use all-digit passwords."
	},
	{
		"^[a-z]{1,9}$",
		REG_EXTENDED|REG_NOSUB,
		"Please don't use an all-lower case password."
	},
	{
		"^[a-z]{1,6}[0-9]+$",
		REG_EXTENDED|REG_NOSUB|REG_ICASE,
		"Please use a more complicated password."
	},
	{
		"^([a-z][0-9]){1,4}$",
		REG_EXTENDED|REG_NOSUB|REG_ICASE,
		"Please use a more complicated password."
	},
	{
		"^([0-9][a-z]){1,4}$",
		REG_EXTENDED|REG_NOSUB|REG_ICASE,
		"Please use a more complicated password."
	}
};

int
pwd_check(struct passwd *pwd, char *password)
{
	regex_t rgx;
	int i, res;
	char    option[LINE_MAX];
	int pipefds[2];
	
	if (strlen(password) <= 5) {
		printf("Please enter a longer password.\n");
		return (0);
	}

	for (i = 0; i < sizeof(patterns)/sizeof(struct pattern); i++) {
		if (regcomp(&rgx, patterns[i].match, patterns[i].flags) != 0)
			continue;
		res = regexec(&rgx, password, 0, NULL, 0);
		regfree(&rgx);
		if (!res) {
			printf("%s\nUnusual capitalization, control characters or digits are suggested.\n", patterns[i].response);
			return (0);
		}
	}

	/* Okay, now pass control to an external program */
	pw_getconf(option, LINE_MAX, pwd->pw_name, "pwdcheck");

	/* Try to find an entry for the group */
	if (*option == 0) {
	        struct group *grp;
	        char grpkey[LINE_MAX];

	        grp = getgrgid(pwd->pw_gid);
	        if (grp != NULL) {
                        snprintf(grpkey, LINE_MAX-1, ".%s", grp->gr_name);
			grpkey[LINE_MAX-1] = 0;
			pw_getconf(option, LINE_MAX, grpkey, "pwdcheck");
		}
		if (*option == 0)
		        pw_getconf(option, LINE_MAX, "default", "pwdcheck");
	}
	
	/* If no checker is specified, we accept the password */
	if (*option == 0)
		return (1);

	if (pipe(pipefds) == -1) {
		warn("pipe");
		goto out;
	}

	res = fork();
	if (res == 0) {
		char *argp[] = { "sh", "-c", NULL, NULL};
		
		/* Drop privileges */
		seteuid(getuid());
		setuid(getuid());

		if (dup2(pipefds[0], STDIN_FILENO) == -1)
			exit(1);

		close(pipefds[0]);
		close(pipefds[1]);

		argp[2] = option;
		if (execv(_PATH_BSHELL, argp) == -1)
			exit(1);
		/* NOT REACHED */
	} else if (res == -1) {
		warn("fork");
		goto out;
	}
	close(pipefds[0]);

	/* Send the password to STDIN of child */
	write(pipefds[1], password, strlen(password) + 1);
	close(pipefds[1]);

	/* get the return value from the child */
	wait(&res);
	if (WIFEXITED(res) && WEXITSTATUS(res) == 0)
		return (1);

 out:
	printf("Please use a different password.\nUnusual capitalization, control characters or digits are suggested.\n");
	return (0);
}

int pwd_gettries( struct passwd *pwd ) {
	char option[LINE_MAX];
	char *ep = option; 

	pw_getconf(option, LINE_MAX, pwd->pw_name, "pwdtries");

	/* Try to find an entry for the group */
	if (*option == 0) {
	        struct group *grp;
	        char grpkey[LINE_MAX];

	        grp = getgrgid(pwd->pw_gid);
	        if (grp != NULL) {
                        snprintf(grpkey, LINE_MAX-1, ".%s", grp->gr_name);
			grpkey[LINE_MAX-1] = 0;
			pw_getconf(option, LINE_MAX, grpkey, "pwdtries");
		}
		if (*option == 0)
		        pw_getconf(option, LINE_MAX, "default", "pwdtries");
	}
	
	if (*option == 0)
		goto out;
	else {
		long lval;
		errno = 0;
		lval = strtol(option, &ep, 10);
		if (option[0] == '\0' || *ep != '\0') {
			fprintf(stderr, 
				"Warning: Bad pwdtries line in /etc/passwd.conf");   
			goto out;
		}
		if ((errno == ERANGE && (lval == LONG_MAX
					 || lval == LONG_MIN)) ||
		    (lval > INT_MAX || lval < INT_MIN)) {
			fprintf(stderr, 
				"Warning: pwdtries out of range in /etc/passwd.conf");   
			goto out;
		}
		return((int) lval);
	}

	/* If no amount of tries is specified, return a default of 
	 * 3, meaning that after 3 attempts where the user is foiled
	 * by the password checks, it will no longer be checked and
	 * they can set it to whatever they like.
	 */
	out:
		return (3);
}
  

