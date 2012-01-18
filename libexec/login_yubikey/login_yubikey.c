/* $OpenBSD$ */
/* $Id: login_yubikey.c,v 1.1.1.1 2010/03/16 14:13:08 dhartmei Exp $ */

/*
 * Copyright (c) 2010 Daniel Hartmeier <daniel@benzedrine.cx>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <ctype.h>
#include <login_cap.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "yubikey.h"

#define	MODE_LOGIN	0
#define	MODE_CHALLENGE	1
#define	MODE_RESPONSE	2

#define	AUTH_OK		0
#define	AUTH_FAILED	-1

static const char *path = "/var/db/yubikey";

static int clean_string(const char *);
static int yubikey_login(const char *, const char *);

int main(int argc, char *argv[])
{
	int ch, ret, mode = MODE_LOGIN;
	FILE *f = NULL;
	char *username, *password = NULL;
	char response[1024];

	setpriority(PRIO_PROCESS, 0, 0);
	openlog(NULL, LOG_ODELAY, LOG_AUTH);

	while ((ch = getopt(argc, argv, "dv:s:")) != -1) {
		switch (ch) {
		case 'd':
			f = stdout;
			break;
		case 'v':
			syslog(LOG_INFO, "-v %s", optarg);
			break;
		case 's':
			if (!strcmp(optarg, "login"))
				mode = MODE_LOGIN;
			else if (!strcmp(optarg, "response"))
				mode = MODE_RESPONSE;
			else if (!strcmp(optarg, "challenge"))
				mode = MODE_CHALLENGE;
			else {
				syslog(LOG_ERR, "%s: invalid service", optarg);
				exit(EXIT_FAILURE);
			}
			break;
		default:
			syslog(LOG_ERR, "usage error1");
			exit(EXIT_FAILURE);
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 2 && argc != 1) {
		syslog(LOG_ERR, "usage error2");
		exit(EXIT_FAILURE);
	}
	username = argv[0];
	/* passed by sshd(8) for non-existing users */
	if (!strcmp(username, "NOUSER"))
		exit(EXIT_FAILURE);
	if (!clean_string(username)) {
		syslog(LOG_ERR, "clean_string username");
		exit(EXIT_FAILURE);
	}

	if (f == NULL && (f = fdopen(3, "r+")) == NULL) {
		syslog(LOG_ERR, "user %s: fdopen: %m", username);
		exit(EXIT_FAILURE);
	}

	switch (mode) {
		case MODE_LOGIN:
			if ((password = getpass("Password:")) == NULL) {
				syslog(LOG_ERR, "user %s: getpass: %m",
				    username);
				exit(EXIT_FAILURE);
			}
			break;
		case MODE_CHALLENGE:
			/* see login.conf(5) section CHALLENGES */
			fprintf(f, "%s\n", BI_SILENT);
			exit(EXIT_SUCCESS);
			break;
		case MODE_RESPONSE: {
			/* see login.conf(5) section RESPONSES */
			/* this happens e.g. when called from sshd(8) */
			int count;
			mode = 0;
			count = -1;
			while (++count < sizeof(response) &&
			    read(3, &response[count], (size_t)1) ==
			    (ssize_t)1) {
				if (response[count] == '\0' && ++mode == 2)
					break;
				if (response[count] == '\0' && mode == 1) {
					password = response + count + 1;
				}
			}
			if (mode < 2) {
				syslog(LOG_ERR, "user %s: protocol error "
				    "on back channel", username);
				exit(EXIT_FAILURE);
			}
			break;
		}
	}

	ret = yubikey_login(username, password);
	memset(password, 0, strlen(password));
	if (ret == AUTH_OK) {
		syslog(LOG_INFO, "user %s: authorize", username);
		fprintf(f, "%s\n", BI_AUTH);
	} else {
		syslog(LOG_INFO, "user %s: reject", username);
		fprintf(f, "%s\n", BI_REJECT);
	}
	closelog();
	return (EXIT_SUCCESS);
}

static int
clean_string(const char *s)
{
	while (*s) {
		if (!isalnum(*s) && *s != '-' && *s != '_')
			return (0);
		++s;
	}
	return (1);
}

static int
yubikey_login(const char *username, const char *password)
{
	char fn[MAXPATHLEN];
	char hexkey[33], key[YUBIKEY_KEY_SIZE];
	char hexuid[13], uid[YUBIKEY_UID_SIZE];
	FILE *f;
	yubikey_token_st tok;
	u_int32_t last_ctr = 0, ctr;

	if (strlen(password) > 32)
		password = password + strlen(password) - 32;
	if (strlen(password) != 32) {
		syslog(LOG_INFO, "user %s: password %s: len %d != 32",
		    username, password, (int)strlen(password));
		return (AUTH_FAILED);
	}

	snprintf(fn, sizeof(fn), "%s/%s.uid", path, username);
	if ((f = fopen(fn, "r")) == NULL) {
		syslog(LOG_ERR, "user %s: fopen: %s: %m", username, fn);
		return (AUTH_FAILED);
	}
	if (fscanf(f, "%12s", hexuid) != 1) {
		syslog(LOG_ERR, "user %s: fscanf: %s: %m", username, fn);
		fclose(f);
		return (AUTH_FAILED);
	}
	fclose(f);

	snprintf(fn, sizeof(fn), "%s/%s.key", path, username);
	if ((f = fopen(fn, "r")) == NULL) {
		syslog(LOG_ERR, "user %s: fopen: %s: %m", username, fn);
		return (AUTH_FAILED);
	}
	if (fscanf(f, "%32s", hexkey) != 1) {
		syslog(LOG_ERR, "user %s: fscanf: %s: %m", username, fn);
		fclose(f);
		return (AUTH_FAILED);
	}
	fclose(f);
	if (strlen(hexkey) != 32) {
		syslog(LOG_ERR, "user %s: key len != 32", username);
		return (AUTH_FAILED);
	}

	snprintf(fn, sizeof(fn), "%s/%s.ctr", path, username);
	if ((f = fopen(fn, "r")) != NULL) {
		if (fscanf(f, "%u", &last_ctr) != 1)
			last_ctr = 0;
		fclose(f);
	}

	yubikey_hex_decode(uid, hexuid, YUBIKEY_UID_SIZE);
	yubikey_hex_decode(key, hexkey, YUBIKEY_KEY_SIZE);
	yubikey_parse((uint8_t *)password, (uint8_t *)key, &tok);
	if (!yubikey_crc_ok_p((uint8_t *)&tok)) {
		syslog(LOG_INFO, "user %s: crc %04x failed: %s",
		    username, tok.crc, password);
		return (AUTH_FAILED);
	}
	syslog(LOG_INFO, "user %s: crc %04x ok", username, tok.crc);

	if (memcmp(tok.uid, uid, YUBIKEY_UID_SIZE)) {
		char h[13];

		yubikey_hex_encode(h, (const char *)tok.uid, YUBIKEY_UID_SIZE);
		syslog(LOG_INFO, "user %s: uid %s != %s", username, h, hexuid);
		return (AUTH_FAILED);
	}
	syslog(LOG_INFO, "user %s: uid %s matches", username, hexuid);

	ctr = ((u_int32_t)yubikey_counter(tok.ctr) << 8) | tok.use;
	if (ctr <= last_ctr) {
		syslog(LOG_INFO, "user %s: counter %u.%u <= %u.%u "
		    "(REPLAY ATTACK!)", username, ctr / 256, ctr % 256,
		    last_ctr / 256, last_ctr % 256);
		return (AUTH_FAILED);
	}
	syslog(LOG_INFO, "user %s: counter %u.%u > %u.%u",
	    username, ctr / 256, ctr % 256, last_ctr / 256, last_ctr % 256);
	if ((f = fopen(fn, "w")) == NULL) {
		syslog(LOG_ERR, "user %s: fopen: %s: %m", username, fn);
		return (AUTH_FAILED);
	}
	fprintf(f, "%u", ctr);
	fclose(f);

	return (AUTH_OK);
}
