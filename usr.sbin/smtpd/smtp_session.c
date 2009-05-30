/*	$OpenBSD: smtp_session.c,v 1.102 2009/05/28 08:50:08 jacekm Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2008-2009 Jacek Masiulaniec <jacekm@dobremiasto.net>
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

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/param.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <event.h>
#include <pwd.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <keynote.h>

#include "smtpd.h"

int	 	 session_rfc5321_helo_handler(struct session *, char *);
int		 session_rfc5321_ehlo_handler(struct session *, char *);
int		 session_rfc5321_rset_handler(struct session *, char *);
int		 session_rfc5321_noop_handler(struct session *, char *);
int		 session_rfc5321_data_handler(struct session *, char *);
int		 session_rfc5321_mail_handler(struct session *, char *);
int		 session_rfc5321_rcpt_handler(struct session *, char *);
int		 session_rfc5321_vrfy_handler(struct session *, char *);
int		 session_rfc5321_expn_handler(struct session *, char *);
int		 session_rfc5321_turn_handler(struct session *, char *);
int		 session_rfc5321_help_handler(struct session *, char *);
int		 session_rfc5321_quit_handler(struct session *, char *);
int		 session_rfc5321_none_handler(struct session *, char *);

int		 session_rfc1652_mail_handler(struct session *, char *);

int		 session_rfc3207_stls_handler(struct session *, char *);

int		 session_rfc4954_auth_handler(struct session *, char *);
void		 session_rfc4954_auth_plain(struct session *, char *);
void		 session_rfc4954_auth_login(struct session *, char *);

void		 session_read(struct bufferevent *, void *);
void		 session_read_data(struct session *, char *, size_t);
void		 session_write(struct bufferevent *, void *);
void		 session_error(struct bufferevent *, short event, void *);
void		 session_command(struct session *, char *, size_t);
char		*session_readline(struct session *, size_t *);
void		 session_respond_delayed(int, short, void *);
int		 session_set_path(struct path *, char *);
void		 session_imsg(struct session *, enum smtp_proc_type,
		     enum imsg_type, u_int32_t, pid_t, int, void *, u_int16_t);

struct session_cmd {
	char	 *name;
	int		(*func)(struct session *, char *);
};

struct session_cmd rfc5321_cmdtab[] = {
	{ "helo",	session_rfc5321_helo_handler },
	{ "ehlo",	session_rfc5321_ehlo_handler },
	{ "rset",	session_rfc5321_rset_handler },
	{ "noop",	session_rfc5321_noop_handler },
	{ "data",	session_rfc5321_data_handler },
	{ "mail from",	session_rfc5321_mail_handler },
	{ "rcpt to",	session_rfc5321_rcpt_handler },
	{ "vrfy",	session_rfc5321_vrfy_handler },
	{ "expn",	session_rfc5321_expn_handler },
	{ "turn",	session_rfc5321_turn_handler },
	{ "help",	session_rfc5321_help_handler },
	{ "quit",	session_rfc5321_quit_handler }
};

struct session_cmd rfc1652_cmdtab[] = {
	{ "mail from",	session_rfc1652_mail_handler },
};

struct session_cmd rfc3207_cmdtab[] = {
	{ "starttls",	session_rfc3207_stls_handler }
};

struct session_cmd rfc4954_cmdtab[] = {
	{ "auth",	session_rfc4954_auth_handler }
};

int
session_rfc3207_stls_handler(struct session *s, char *args)
{
	if (! ADVERTISE_TLS(s))
		return 0;

	if (s->s_state == S_GREETED) {
		session_respond(s, "503 Polite people say HELO first");
		return 1;
	}

	if (s->s_state != S_HELO) {
		session_respond(s, "503 TLS not allowed at this stage");
		return 1;
	}

	if (args != NULL) {
		session_respond(s, "501 No parameters allowed");
		return 1;
	}

	s->s_state = S_TLS;
	session_respond(s, "220 Ready to start TLS");

	return 1;
}

int
session_rfc4954_auth_handler(struct session *s, char *args)
{
	char	*method;
	char	*eom;

	if (! ADVERTISE_AUTH(s)) {
		if (s->s_flags & F_AUTHENTICATED) {
			session_respond(s, "503 Already authenticated");
			return 1;
		} else
			return 0;
	}

	if (s->s_state == S_GREETED) {
		session_respond(s, "503 Polite people say HELO first");
		return 1;
	}

	if (args == NULL) {
		session_respond(s, "501 No parameters given");
		return 1;
	}

	method = args;
	eom = strchr(args, ' ');
	if (eom == NULL)
		eom = strchr(args, '\t');
	if (eom != NULL)
		*eom++ = '\0';

	if (strcasecmp(method, "PLAIN") == 0)
		session_rfc4954_auth_plain(s, eom);
	else if (strcasecmp(method, "LOGIN") == 0)
		session_rfc4954_auth_login(s, eom);
	else
		session_respond(s, "504 AUTH method '%s' unsupported", method);

	return 1;
}

void
session_rfc4954_auth_plain(struct session *s, char *arg)
{
	struct auth	*a = &s->s_auth;
	char		 buf[1024], *user, *pass;
	int		 len;

	switch (s->s_state) {
	case S_HELO:
		if (arg == NULL) {
			s->s_state = S_AUTH_INIT;
			session_respond(s, "334 ");
			return;
		}
		s->s_state = S_AUTH_INIT;
		/* FALLTHROUGH */

	case S_AUTH_INIT:
		/* String is not NUL terminated, leave room. */
		if ((len = kn_decode_base64(arg, buf, sizeof(buf) - 1)) == -1)
			goto abort;
		/* buf is a byte string, NUL terminate. */
		buf[len] = '\0';

		/*
		 * Skip "foo" in "foo\0user\0pass", if present.
		 */
		user = memchr(buf, '\0', len);
		if (user == NULL || user >= buf + len - 2)
			goto abort;
		user++; /* skip NUL */
		if (strlcpy(a->user, user, sizeof(a->user)) >= sizeof(a->user))
			goto abort;

		pass = memchr(user, '\0', len - (user - buf));
		if (pass == NULL || pass >= buf + len - 2)
			goto abort;
		pass++; /* skip NUL */
		if (strlcpy(a->pass, pass, sizeof(a->pass)) >= sizeof(a->pass))
			goto abort;

		s->s_state = S_AUTH_FINALIZE;

		a->id = s->s_id;
		session_imsg(s, PROC_PARENT, IMSG_PARENT_AUTHENTICATE, 0, 0, -1,
		    a, sizeof(*a));

		bzero(a->pass, sizeof(a->pass));
		return;

	default:
		fatal("session_rfc4954_auth_plain: unknown state");
	}

abort:
	session_respond(s, "501 Syntax error");
	s->s_state = S_HELO;
}

void
session_rfc4954_auth_login(struct session *s, char *arg)
{
	struct auth	*a = &s->s_auth;

	switch (s->s_state) {
	case S_HELO:
		s->s_state = S_AUTH_USERNAME;
		session_respond(s, "334 VXNlcm5hbWU6");
		return;

	case S_AUTH_USERNAME:
		bzero(a->user, sizeof(a->user));
		if (kn_decode_base64(arg, a->user, sizeof(a->user) - 1) == -1)
			goto abort;

		s->s_state = S_AUTH_PASSWORD;
		session_respond(s, "334 UGFzc3dvcmQ6");
		return;

	case S_AUTH_PASSWORD:
		bzero(a->pass, sizeof(a->pass));
		if (kn_decode_base64(arg, a->pass, sizeof(a->pass) - 1) == -1)
			goto abort;

		s->s_state = S_AUTH_FINALIZE;

		a->id = s->s_id;
		session_imsg(s, PROC_PARENT, IMSG_PARENT_AUTHENTICATE, 0, 0, -1,
		    a, sizeof(*a));

		bzero(a->pass, sizeof(a->pass));
		return;
	
	default:
		fatal("session_rfc4954_auth_login: unknown state");
	}

abort:
	session_respond(s, "501 Syntax error");
	s->s_state = S_HELO;
}

int
session_rfc1652_mail_handler(struct session *s, char *args)
{
	char *body;

	if (s->s_state == S_GREETED) {
		session_respond(s, "503 Polite people say HELO first");
		return 1;
	}

	body = strrchr(args, ' ');
	if (body != NULL) {
		*body++ = '\0';

		if (strcasecmp("body=7bit", body) == 0) {
			s->s_flags &= ~F_8BITMIME;
		}

		else if (strcasecmp("body=8bitmime", body) != 0) {
			session_respond(s, "503 Invalid BODY");
			return 1;
		}

		return session_rfc5321_mail_handler(s, args);
	}

	return 0;
}

int
session_rfc5321_helo_handler(struct session *s, char *args)
{
	if (args == NULL) {
		session_respond(s, "501 HELO requires domain address");
		return 1;
	}

	if (strlcpy(s->s_msg.session_helo, args, sizeof(s->s_msg.session_helo))
	    >= sizeof(s->s_msg.session_helo)) {
		session_respond(s, "501 Invalid domain name");
		return 1;
	}

	s->s_state = S_HELO;
	s->s_flags &= F_SECURE|F_AUTHENTICATED;

	session_respond(s, "250 %s Hello %s [%s], pleased to meet you",
	    s->s_env->sc_hostname, args, ss_to_text(&s->s_ss));

	return 1;
}

int
session_rfc5321_ehlo_handler(struct session *s, char *args)
{
	if (args == NULL) {
		session_respond(s, "501 EHLO requires domain address");
		return 1;
	}

	if (strlcpy(s->s_msg.session_helo, args, sizeof(s->s_msg.session_helo))
	    >= sizeof(s->s_msg.session_helo)) {
		session_respond(s, "501 Invalid domain name");
		return 1;
	}

	s->s_state = S_HELO;
	s->s_flags &= F_SECURE|F_AUTHENTICATED;
	s->s_flags |= F_EHLO;
	s->s_flags |= F_8BITMIME;

	session_respond(s, "250-%s Hello %s [%s], pleased to meet you",
	    s->s_env->sc_hostname, args, ss_to_text(&s->s_ss));
	session_respond(s, "250-8BITMIME");

	if (ADVERTISE_TLS(s))
		session_respond(s, "250-STARTTLS");

	if (ADVERTISE_AUTH(s))
		session_respond(s, "250-AUTH PLAIN LOGIN");

	session_respond(s, "250 HELP");

	return 1;
}

int
session_rfc5321_rset_handler(struct session *s, char *args)
{
	s->s_state = S_HELO;
	session_respond(s, "250 Reset state");

	return 1;
}

int
session_rfc5321_noop_handler(struct session *s, char *args)
{
	session_respond(s, "250 OK");

	return 1;
}

int
session_rfc5321_mail_handler(struct session *s, char *args)
{
	if (s->s_state == S_GREETED) {
		session_respond(s, "503 Polite people say HELO first");
		return 1;
	}

	if (s->s_state != S_HELO) {
		session_respond(s, "503 Sender already specified");
		return 1;
	}

	if (! session_set_path(&s->s_msg.sender, args)) {
		/* No need to even transmit to MFA, path is invalid */
		session_respond(s, "553 Sender address syntax error");
		return 1;
	}

	s->rcptcount = 0;
	s->s_state = S_MAIL_MFA;
	s->s_msg.id = s->s_id;
	s->s_msg.session_id = s->s_id;
	s->s_msg.session_ss = s->s_ss;

	log_debug("session_rfc5321_mail_handler: sending notification to mfa");

	session_imsg(s, PROC_MFA, IMSG_MFA_MAIL, 0, 0, -1, &s->s_msg,
	    sizeof(s->s_msg));
	return 1;
}

int
session_rfc5321_rcpt_handler(struct session *s, char *args)
{
	if (s->s_state == S_GREETED) {
		session_respond(s, "503 Polite people say HELO first");
		return 1;
	}

	if (s->s_state == S_HELO) {
		session_respond(s, "503 Need MAIL before RCPT");
		return 1;
	}

	if (! session_set_path(&s->s_msg.session_rcpt, args)) {
		/* No need to even transmit to MFA, path is invalid */
		session_respond(s, "553 Recipient address syntax error");
		return 1;
	}

	s->s_state = S_RCPT_MFA;

	session_imsg(s, PROC_MFA, IMSG_MFA_RCPT, 0, 0, -1, &s->s_msg,
	    sizeof(s->s_msg));
	return 1;
}

int
session_rfc5321_quit_handler(struct session *s, char *args)
{
	session_respond(s, "221 %s Closing connection", s->s_env->sc_hostname);

	s->s_flags |= F_QUIT;

	return 1;
}

int
session_rfc5321_data_handler(struct session *s, char *args)
{
	if (s->s_state == S_GREETED) {
		session_respond(s, "503 Polite people say HELO first");
		return 1;
	}

	if (s->s_state == S_HELO) {
		session_respond(s, "503 Need MAIL before DATA");
		return 1;
	}

	if (s->s_state == S_MAIL) {
		session_respond(s, "503 Need RCPT before DATA");
		return 1;
	}

	s->s_state = S_DATA_QUEUE;

	session_imsg(s, PROC_QUEUE, IMSG_QUEUE_MESSAGE_FILE, 0, 0, -1,
	    &s->s_msg, sizeof(s->s_msg));

	return 1;
}

int
session_rfc5321_vrfy_handler(struct session *s, char *args)
{
	session_respond(s, "252 Cannot VRFY; try RCPT to attempt delivery");

	return 1;
}

int
session_rfc5321_expn_handler(struct session *s, char *args)
{
	session_respond(s, "502 Sorry, we do not allow this operation");

	return 1;
}

int
session_rfc5321_turn_handler(struct session *s, char *args)
{
	session_respond(s, "502 Sorry, we do not allow this operation");

	return 1;
}

int
session_rfc5321_help_handler(struct session *s, char *args)
{
	session_respond(s, "214- This is OpenSMTPD");
	session_respond(s, "214- To report bugs in the implementation, please "
	    "contact bugs@openbsd.org");
	session_respond(s, "214- with full details");
	session_respond(s, "214 End of HELP info");

	return 1;
}

void
session_command(struct session *s, char *cmd, size_t nr)
{
	char		*ep, *args;
	unsigned int	 i;

	if (nr > SMTP_CMDLINE_MAX) {
		session_respond(s, "500 Line too long");
		return;
	}

	if ((ep = strchr(cmd, ':')) == NULL)
		ep = strchr(cmd, ' ');
	if (ep != NULL) {
		*ep = '\0';
		args = ++ep;
		while (isspace((int)*args))
			args++;
	} else
		args = NULL;

	log_debug("command: %s\targs: %s", cmd, args);

	if (!(s->s_flags & F_EHLO))
		goto rfc5321;

	/* RFC 1652 - 8BITMIME */
	for (i = 0; i < nitems(rfc1652_cmdtab); ++i)
		if (strcasecmp(rfc1652_cmdtab[i].name, cmd) == 0)
			break;
	if (i < nitems(rfc1652_cmdtab)) {
		if (rfc1652_cmdtab[i].func(s, args))
			return;
	}

	/* RFC 3207 - STARTTLS */
	for (i = 0; i < nitems(rfc3207_cmdtab); ++i)
		if (strcasecmp(rfc3207_cmdtab[i].name, cmd) == 0)
			break;
	if (i < nitems(rfc3207_cmdtab)) {
		if (rfc3207_cmdtab[i].func(s, args))
			return;
	}

	/* RFC 4954 - AUTH */
	for (i = 0; i < nitems(rfc4954_cmdtab); ++i)
		if (strcasecmp(rfc4954_cmdtab[i].name, cmd) == 0)
			break;
	if (i < nitems(rfc4954_cmdtab)) {
		if (rfc4954_cmdtab[i].func(s, args))
			return;
	}

rfc5321:
	/* RFC 5321 - SMTP */
	for (i = 0; i < nitems(rfc5321_cmdtab); ++i)
		if (strcasecmp(rfc5321_cmdtab[i].name, cmd) == 0)
			break;
	if (i < nitems(rfc5321_cmdtab)) {
		if (rfc5321_cmdtab[i].func(s, args))
			return;
	}

	session_respond(s, "500 Command unrecognized");
}

void
session_pickup(struct session *s, struct submit_status *ss)
{
	if (s == NULL)
		fatal("session_pickup: desynchronized");

	if ((ss != NULL && ss->code == 421) ||
	    (s->s_msg.status & S_MESSAGE_TEMPFAILURE)) {
		session_respond(s, "421 Service temporarily unavailable");
		s->s_env->stats->smtp.tempfail++;
		s->s_flags |= F_QUIT;
		return;
	}

	switch (s->s_state) {
	case S_INIT:
		s->s_state = S_GREETED;
		log_debug("session_pickup: greeting client");
		session_respond(s, SMTPD_BANNER, s->s_env->sc_hostname);
		break;

	case S_TLS:
		if (s->s_flags & F_WRITEONLY)
			fatalx("session_pickup: corrupt session");
		bufferevent_enable(s->s_bev, EV_READ);
		s->s_state = S_GREETED;
		break;

	case S_AUTH_FINALIZE:
		if (s->s_flags & F_AUTHENTICATED)
			session_respond(s, "235 Authentication succeeded");
		else
			session_respond(s, "535 Authentication failed");
		s->s_state = S_HELO;
		break;

	case S_MAIL_MFA:
		if (ss == NULL)
			fatalx("bad ss at S_MAIL_MFA");
		if (ss->code != 250) {
			s->s_state = S_HELO;
			session_respond(s, "%d Sender rejected", ss->code);
			return;
		}

		s->s_state = S_MAIL_QUEUE;
		s->s_msg.sender = ss->u.path;

		session_imsg(s, PROC_QUEUE, IMSG_QUEUE_CREATE_MESSAGE, 0, 0, -1,
		    &s->s_msg, sizeof(s->s_msg));
		break;

	case S_MAIL_QUEUE:
		if (ss == NULL)
			fatalx("bad ss at S_MAIL_QUEUE");
		s->s_state = S_MAIL;
		session_respond(s, "%d Sender ok", ss->code);
		break;

	case S_RCPT_MFA:
		if (ss == NULL)
			fatalx("bad ss at S_RCPT_MFA");
		/* recipient was not accepted */
		if (ss->code != 250) {
			/* We do not have a valid recipient, downgrade state */
			if (s->rcptcount == 0)
				s->s_state = S_MAIL;
			else
				s->s_state = S_RCPT;
			session_respond(s, "%d Recipient rejected", ss->code);
			return;
		}

		s->s_state = S_RCPT;
		s->rcptcount++;
		s->s_msg.recipient = ss->u.path;

		session_respond(s, "%d Recipient ok", ss->code);
		break;

	case S_DATA_QUEUE:
		s->s_state = S_DATACONTENT;
		session_respond(s, "354 Enter mail, end with \".\" on a line by"
		    " itself");

		fprintf(s->datafp, "Received: from %s (%s [%s])\n",
		    s->s_msg.session_helo, s->s_hostname, ss_to_text(&s->s_ss));
		fprintf(s->datafp, "\tby %s (OpenSMTPD) with %sSMTP id %s",
		    s->s_env->sc_hostname, s->s_flags & F_EHLO ? "E" : "",
		    s->s_msg.message_id);

		if (s->rcptcount == 1)
			fprintf(s->datafp, "\n\tfor <%s@%s>; ",
			    s->s_msg.session_rcpt.user,
			    s->s_msg.session_rcpt.domain);
		else
			fprintf(s->datafp, ";\n\t");

		fprintf(s->datafp, "%s\n", time_to_text(time(NULL)));
		break;

	case S_DONE:
		session_respond(s, "250 %s Message accepted for delivery",
		    s->s_msg.message_id);
		log_info("%s: from=<%s@%s>, size=%ld, nrcpts=%zd, proto=%s, "
		    "relay=%s [%s]",
		    s->s_msg.message_id,
		    s->s_msg.sender.user,
		    s->s_msg.sender.domain,
		    s->s_datalen,
		    s->rcptcount,
		    s->s_flags & F_EHLO ? "ESMTP" : "SMTP",
		    s->s_hostname,
		    ss_to_text(&s->s_ss));

		s->s_state = S_HELO;
		s->s_msg.message_id[0] = '\0';
		s->s_msg.message_uid[0] = '\0';
		bzero(&s->s_nresp, sizeof(s->s_nresp));
		break;

	default:
		fatal("session_pickup: unknown state");
	}
}

void
session_init(struct listener *l, struct session *s)
{
	s->s_state = S_INIT;

	if (l->flags & F_SMTPS) {
		ssl_session_init(s);
		return;
	}

	session_bufferevent_new(s);
	session_pickup(s, NULL);
}

void
session_bufferevent_new(struct session *s)
{
	if (s->s_bev != NULL)
		fatalx("session_bufferevent_new: attempt to override existing "
		    "bufferevent");

	if (s->s_flags & F_WRITEONLY)
		fatalx("session_bufferevent_new: corrupt session");

	s->s_bev = bufferevent_new(s->s_fd, session_read, session_write,
	    session_error, s);
	if (s->s_bev == NULL)
		fatal("session_bufferevent_new");

	bufferevent_settimeout(s->s_bev, SMTPD_SESSION_TIMEOUT,
	    SMTPD_SESSION_TIMEOUT);
}

void
session_read(struct bufferevent *bev, void *p)
{
	struct session	*s = p;
	char		*line;
	size_t		 nr;

	for (;;) {
		line = session_readline(s, &nr);
		if (line == NULL)
			return;

		switch (s->s_state) {
		case S_AUTH_INIT:
			if (s->s_msg.status & S_MESSAGE_TEMPFAILURE)
				goto tempfail;
			session_rfc4954_auth_plain(s, line);
			break;

		case S_AUTH_USERNAME:
		case S_AUTH_PASSWORD:
			if (s->s_msg.status & S_MESSAGE_TEMPFAILURE)
				goto tempfail;
			session_rfc4954_auth_login(s, line);
			break;

		case S_GREETED:
		case S_HELO:
		case S_MAIL:
		case S_RCPT:
			if (s->s_msg.status & S_MESSAGE_TEMPFAILURE)
				goto tempfail;
			session_command(s, line, nr);
			break;

		case S_DATACONTENT:
			session_read_data(s, line, nr);
			break;

		default:
			fatalx("session_read: unexpected state");
		}

		free(line);
	}
	return;

tempfail:
	session_respond(s, "421 Service temporarily unavailable");
	s->s_env->stats->smtp.tempfail++;
	s->s_flags |= F_QUIT;
	free(line);
}

void
session_read_data(struct session *s, char *line, size_t nread)
{
	size_t len;
	size_t i;

	if (strcmp(line, ".") == 0) {
		s->s_datalen = ftell(s->datafp);
		if (! safe_fclose(s->datafp))
			s->s_msg.status |= S_MESSAGE_TEMPFAILURE;
		s->datafp = NULL;

		if (s->s_msg.status & S_MESSAGE_PERMFAILURE) {
			session_respond(s, "554 Transaction failed");
			s->s_state = S_HELO;
		} else if (s->s_msg.status & S_MESSAGE_TEMPFAILURE) {
			session_respond(s, "421 Temporary failure");
			s->s_flags |= F_QUIT;
			s->s_env->stats->smtp.tempfail++;
		} else {
			session_imsg(s, PROC_QUEUE, IMSG_QUEUE_COMMIT_MESSAGE,
			    0, 0, -1, &s->s_msg, sizeof(s->s_msg));
			s->s_state = S_DONE;
		}

		return;
	}

	/* Don't waste resources on message if it's going to bin anyway. */
	if (s->s_msg.status & (S_MESSAGE_PERMFAILURE|S_MESSAGE_TEMPFAILURE))
		return;

	if (nread > SMTP_TEXTLINE_MAX) {
		s->s_msg.status |= S_MESSAGE_PERMFAILURE;
		return;
	}

	/* "If the first character is a period and there are other characters
	 *  on the line, the first character is deleted." [4.5.2]
	 */
	if (*line == '.')
		line++;

	len = strlen(line);

	if (fwrite(line, len, 1, s->datafp) != 1 ||
	    fwrite("\n", 1, 1, s->datafp) != 1) {
		s->s_msg.status |= S_MESSAGE_TEMPFAILURE;
		return;
	}

	if (! (s->s_flags & F_8BITMIME)) {
		for (i = 0; i < len; ++i)
			if (line[i] & 0x80)
				break;
		if (i != len) {
			s->s_msg.status |= S_MESSAGE_PERMFAILURE;
			return;
		}
	}
}

void
session_write(struct bufferevent *bev, void *p)
{
	struct session	*s = p;

	if (s->s_flags & F_WRITEONLY) {
		/*
		 * Finished writing to a session that is waiting for an IMSG
		 * response, therefore can't destroy session nor re-enable
		 * reading from it.
		 *
		 * If session_respond caller used F_QUIT to request session
		 * destroy after final write, then session will be destroyed
		 * in session_lookup.
		 *
		 * Reading from session will be re-enabled in session_pickup
		 * using another call to session_respond.
		 */
		return;
	} else if (s->s_flags & F_QUIT) {
		/*
		 * session_respond caller requested the session to be dropped.
		 */
		session_destroy(s);
	} else if (s->s_state == S_TLS) {
		/*
		 * Start the TLS conversation.
		 * Destroy the bufferevent as the SSL module re-creates it.
		 */
		bufferevent_free(s->s_bev);
		s->s_bev = NULL;
		ssl_session_init(s);
	} else {
		/*
		 * Common case of responding to client's request.
		 * Re-enable reading from session so that more commands can
		 * be processed.
		 */
		bufferevent_enable(s->s_bev, EV_READ);
	}
}

void
session_error(struct bufferevent *bev, short event, void *p)
{
	struct session	*s = p;
	char		*ip = ss_to_text(&s->s_ss);

	if (event & EVBUFFER_READ) {
		if (event & EVBUFFER_TIMEOUT) {
			log_warnx("client %s read timeout", ip);
			s->s_env->stats->smtp.read_timeout++;
		} else if (event & EVBUFFER_EOF)
			s->s_env->stats->smtp.read_eof++;
		else if (event & EVBUFFER_ERROR) {
			log_warn("client %s read error", ip);
			s->s_env->stats->smtp.read_error++;
		}

		session_destroy(s);
		return;
	}

	if (event & EVBUFFER_WRITE) {
		if (event & EVBUFFER_TIMEOUT) {
			log_warnx("client %s write timeout", ip);
			s->s_env->stats->smtp.write_timeout++;
		} else if (event & EVBUFFER_EOF)
			s->s_env->stats->smtp.write_eof++;
		else if (event & EVBUFFER_ERROR) {
			log_warn("client %s write error", ip);
			s->s_env->stats->smtp.write_error++;
		}

		if (s->s_flags & F_WRITEONLY)
			s->s_flags |= F_QUIT;
		else
			session_destroy(s);
		return;
	}

	fatalx("session_error: unexpected error");
}

void
session_destroy(struct session *s)
{
	log_debug("session_destroy: killing client: %p", s);

	if (s->s_flags & F_WRITEONLY)
		fatalx("session_destroy: corrupt session");

	if (s->datafp != NULL)
		fclose(s->datafp);

	if (s->s_msg.message_id[0] != '\0' && s->s_state != S_DONE)
		imsg_compose(s->s_env->sc_ibufs[PROC_QUEUE],
		    IMSG_QUEUE_REMOVE_MESSAGE, 0, 0, -1, &s->s_msg,
		    sizeof(s->s_msg));

	ssl_session_destroy(s);

	if (s->s_bev != NULL)
		bufferevent_free(s->s_bev);

	if (close(s->s_fd) == -1)
		fatal("session_destroy: close");

	switch (smtpd_process) {
	case PROC_MTA:
		s->s_env->stats->mta.sessions_active--;
		break;
	case PROC_SMTP:
		s->s_env->stats->smtp.sessions_active--;
		if (s->s_env->stats->smtp.sessions_active < s->s_env->sc_maxconn &&
		    !(s->s_msg.flags & F_MESSAGE_ENQUEUED)) {
			/*
			 * if our session_destroy occurs because of a configuration
			 * reload, our listener no longer exist and s->s_l is NULL.
			 */
			if (s->s_l != NULL)
				event_add(&s->s_l->ev, NULL);
		}
		break;
	default:
		fatalx("session_destroy: cannot be called from this process");
	}

	SPLAY_REMOVE(sessiontree, &s->s_env->sc_sessions, s);
	bzero(s, sizeof(*s));
	free(s);
}

char *
session_readline(struct session *s, size_t *nr)
{
	char	*line, *line2;

	*nr = EVBUFFER_LENGTH(s->s_bev->input);
	line = evbuffer_readline(s->s_bev->input);
	if (line == NULL) {
		if (EVBUFFER_LENGTH(s->s_bev->input) > SMTP_ANYLINE_MAX) {
			session_respond(s, "500 Line too long");
			s->s_env->stats->smtp.linetoolong++;
			s->s_flags |= F_QUIT;
		}
		return NULL;
	}
	*nr -= EVBUFFER_LENGTH(s->s_bev->input);

	if (s->s_flags & F_WRITEONLY)
		fatalx("session_readline: corrupt session");
	
	if ((s->s_state != S_DATACONTENT || strcmp(line, ".") == 0) &&
	    (line2 = evbuffer_readline(s->s_bev->input)) != NULL) {
		session_respond(s, "500 Pipelining unsupported");
		s->s_env->stats->smtp.toofast++;
		s->s_flags |= F_QUIT;
		free(line);
		free(line2);

		return NULL;
	}

	return line;
}

int
session_cmp(struct session *s1, struct session *s2)
{
	/*
	 * do not return u_int64_t's
	 */
	if (s1->s_id < s2->s_id)
		return (-1);

	if (s1->s_id > s2->s_id)
		return (1);

	return (0);
}

int
session_set_path(struct path *path, char *line)
{
	size_t len;

	len = strlen(line);
	if (*line != '<' || line[len - 1] != '>')
		return 0;
	line[len - 1] = '\0';

	return recipient_to_path(path, line + 1);
}

void
session_respond(struct session *s, char *fmt, ...)
{
	va_list	 ap;
	int	 n, delay;

	n = EVBUFFER_LENGTH(EVBUFFER_OUTPUT(s->s_bev));

	va_start(ap, fmt);
	if (evbuffer_add_vprintf(EVBUFFER_OUTPUT(s->s_bev), fmt, ap) == -1 ||
	    evbuffer_add_printf(EVBUFFER_OUTPUT(s->s_bev), "\r\n") == -1)
		fatal("session_respond: evbuffer_add_vprintf failed");
	va_end(ap);

	if (smtpd_process == PROC_MTA) {
		bufferevent_enable(s->s_bev, EV_WRITE);
		return;
	}

	bufferevent_disable(s->s_bev, EV_READ);

	/* Detect multi-line response. */
	if (EVBUFFER_LENGTH(EVBUFFER_OUTPUT(s->s_bev)) - n < 4)
		fatalx("session_respond: invalid response length");
	switch (EVBUFFER_DATA(EVBUFFER_OUTPUT(s->s_bev))[n + 3]) {
	case '-':
		return;
	case ' ':
		break;
	default:
		fatalx("session_respond: invalid response");
	}

	/*
	 * Deal with request flooding; avoid letting response rate keep up
	 * with incoming request rate.
	 */
	s->s_nresp[s->s_state]++;

	if (s->s_state == S_RCPT)
		delay = 0;
	else if ((n = s->s_nresp[s->s_state] - FAST_RESPONSES) > 0)
		delay = MIN(1 << (n - 1), MAX_RESPONSE_DELAY);
	else
		delay = 0;

	if (delay > 0) {
		struct timeval tv = { delay, 0 };

		s->s_env->stats->smtp.delays++;
		evtimer_set(&s->s_ev, session_respond_delayed, s);
		evtimer_add(&s->s_ev, &tv);
	} else
		bufferevent_enable(s->s_bev, EV_WRITE);
}

void
session_respond_delayed(int fd, short event, void *p)
{
	struct session	*s = p;

	bufferevent_enable(s->s_bev, EV_WRITE);
}

/*
 * Send IMSG, waiting for reply safely.
 */
void
session_imsg(struct session *s, enum smtp_proc_type proc, enum imsg_type type,
    u_int32_t peerid, pid_t pid, int fd, void *data, u_int16_t datalen)
{
	if (s->s_flags & F_WRITEONLY)
		fatalx("session_imsg: corrupt session");

	/*
	 * Each outgoing IMSG has a response IMSG associated that must be
	 * waited for before the session can be progressed further.
	 * During the wait period:
	 * 1) session must not be destroyed,
	 * 2) session must not be read from,
	 * 3) session may be written to.
	 * Session flag F_WRITEONLY is needed to enforce this policy.
	 *
	 * F_WRITEONLY is cleared in session_lookup.
	 * Reading is re-enabled in session_pickup.
	 */
	s->s_flags |= F_WRITEONLY;
	bufferevent_disable(s->s_bev, EV_READ);
	imsg_compose(s->s_env->sc_ibufs[proc], type, peerid, pid, fd, data,
	    datalen);
}

SPLAY_GENERATE(sessiontree, session, s_nodes, session_cmp);
