/*	$OpenBSD: ypldap.c,v 1.5 2008/09/03 11:04:03 jsg Exp $ */

/*
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
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
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/tree.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <err.h>
#include <event.h>
#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ypldap.h"

__dead void	 usage(void);
int		 check_child(pid_t, const char *);
void		 main_sig_handler(int, short, void *);
void		 main_shutdown(void);
void		 main_dispatch_client(int, short, void *);
void		 main_configure_client(struct env *);
void		 main_init_timer(int, short, void *);
void		 purge_config(struct env *);
void		 reconfigure(struct env *);
void		 set_nonblock(int);

int		 pipe_main2client[2];

pid_t		 client_pid = 0;
char		*conffile = YPLDAP_CONF_FILE;
int		 opts = 0;

void
usage(void)
{
	extern const char	*__progname;

	fprintf(stderr, "usage: %s [-dnv] [-D macro=value] [-f file]\n",
	    __progname);
	exit(1);
}

int
check_child(pid_t pid, const char *pname)
{
	int	status;

	if (waitpid(pid, &status, WNOHANG) > 0) {
		if (WIFEXITED(status)) {
			log_warnx("check_child: lost child %s exited", pname);
			return (1);
		}
		if (WIFSIGNALED(status)) {
			log_warnx("check_child: lost child %s terminated; "
			    "signal %d", pname, WTERMSIG(status));
			return (1);
		}
	}
	return (0);
}

/* ARGUSED */
void
main_sig_handler(int sig, short event, void *p)
{
	int		 die = 0;

	switch (sig) {
	case SIGTERM:
	case SIGINT:
		die = 1;
		/* FALLTHROUGH */
	case SIGCHLD:
		if (check_child(client_pid, "ldap client")) {
			client_pid = 0;
			die = 1;
		}
		if (die)
			main_shutdown();
		break;
	case SIGHUP:
		/* reconfigure */
		break;
	default:
		fatalx("unexpected signal");
	}
}

void
main_shutdown(void)
{
	_exit(0);
}

void
main_dispatch_client(int fd, short event, void *p)
{
	int		 n;
	int		 shut = 0;
	struct env	*env = p;
	struct imsgbuf	*ibuf = env->sc_ibuf;
	struct idm_req	 ir;
	struct imsg	 imsg;

	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read error");
		if (n == 0)
			shut = 1;
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("main_dispatch_client: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_START_UPDATE:
			log_debug("starting directory update");
			env->sc_user_line_len = 0;
			env->sc_group_line_len = 0;
			if ((env->sc_user_names_t = calloc(1,
			    sizeof(*env->sc_user_names_t))) == NULL ||
			    (env->sc_group_names_t = calloc(1,
			    sizeof(*env->sc_group_names_t))) == NULL)
				fatal(NULL);
			RB_INIT(env->sc_user_names_t);
			RB_INIT(env->sc_group_names_t);
			break;
		case IMSG_PW_ENTRY: {
			struct userent	*ue;
			size_t		 len;

			(void)memcpy(&ir, imsg.data, sizeof(ir));
			if ((ue = calloc(1, sizeof(*ue))) == NULL ||
			    (ue->ue_line = strdup(ir.ir_line)) == NULL) {
				/*
				 * should cancel tree update instead.
				 */
				fatal("out of memory");
			}
			ue->ue_uid = ir.ir_key.ik_uid;
			len = strlen(ue->ue_line) + 1;
			ue->ue_line[strcspn(ue->ue_line, ":")] = '\0';
			if (RB_FIND(user_name_tree, env->sc_user_names_t,
			    ue) != NULL) { /* dup */
				free(ue->ue_line);
				free(ue);
				break;
			}
			RB_INSERT(user_name_tree, env->sc_user_names_t, ue);
			env->sc_user_line_len += len;
			break;
		}
		case IMSG_GRP_ENTRY: {
			struct groupent	*ge;
			size_t		 len;

			(void)memcpy(&ir, imsg.data, sizeof(ir));
			if ((ge = calloc(1, sizeof(*ge))) == NULL ||
			    (ge->ge_line = strdup(ir.ir_line)) == NULL) {
				/*
				 * should cancel tree update instead.
				 */
				fatal("out of memory");
			}
			ge->ge_gid = ir.ir_key.ik_gid;
			len = strlen(ge->ge_line) + 1;
			ge->ge_line[strcspn(ge->ge_line, ":")] = '\0';
			if (RB_FIND(group_name_tree, env->sc_group_names_t,
			    ge) != NULL) { /* dup */
				free(ge->ge_line);
				free(ge);
				break;
			}
			RB_INSERT(group_name_tree, env->sc_group_names_t, ge);
			env->sc_group_line_len += len;
			break;
		}
		case IMSG_TRASH_UPDATE: {
			struct userent	*ue;
			struct groupent	*ge;

			while ((ue = RB_ROOT(env->sc_user_names_t)) != NULL) {
				RB_REMOVE(user_name_tree,
				    env->sc_user_names_t, ue);
				free(ue->ue_line);
				free(ue);
			}
			free(env->sc_user_names_t);
			env->sc_user_names_t = NULL;
			while ((ge = RB_ROOT(env->sc_group_names_t))
			    != NULL) {
				RB_REMOVE(group_name_tree,
				    env->sc_group_names_t, ge);
				free(ge->ge_line);
				free(ge);
			}
			free(env->sc_group_names_t);
			env->sc_group_names_t = NULL;
			break;
		}
		case IMSG_END_UPDATE: {
			struct userent	*ue;
			struct groupent	*ge;

			log_debug("updates are over, cleaning up trees now");

			if (env->sc_user_names == NULL) {
				env->sc_user_names = env->sc_user_names_t;
				env->sc_user_lines = NULL;
				env->sc_user_names_t = NULL;

				env->sc_group_names = env->sc_group_names_t;
				env->sc_group_lines = NULL;
				env->sc_group_names_t = NULL;

				flatten_entries(env);
				goto make_uids;
			}

			/*
			 * clean previous tree.
			 */
			while ((ue = RB_ROOT(env->sc_user_names)) != NULL) {
				RB_REMOVE(user_name_tree, env->sc_user_names,
				    ue);
				free(ue);
			}
			free(env->sc_user_names);
			free(env->sc_user_lines);

			env->sc_user_names = env->sc_user_names_t;
			env->sc_user_lines = NULL;
			env->sc_user_names_t = NULL;

			while ((ge = RB_ROOT(env->sc_group_names)) != NULL) {
				RB_REMOVE(group_name_tree,
				    env->sc_group_names, ge);
				free(ge);
			}
			free(env->sc_group_names);
			free(env->sc_group_lines);

			env->sc_group_names = env->sc_group_names_t;
			env->sc_group_lines = NULL;
			env->sc_group_names_t = NULL;


			flatten_entries(env);

			/*
			 * trees are flat now. build up uid and gid trees.
			 */

make_uids:
			RB_INIT(&env->sc_user_uids);
			RB_INIT(&env->sc_group_gids);
			RB_FOREACH(ue, user_name_tree, env->sc_user_names)
				RB_INSERT(user_uid_tree,
				    &env->sc_user_uids, ue);
			RB_FOREACH(ge, group_name_tree, env->sc_group_names)
				RB_INSERT(group_gid_tree,
				    &env->sc_group_gids, ge);
			break;
		}
		default:
			log_debug("main_dispatch_client: unexpected imsg %d",
			   imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	if (!shut)
		imsg_event_add(ibuf);
	else {
		log_debug("king bula sez: ran into dead pipe");
		event_del(&ibuf->ev);
		event_loopexit(NULL);
	}
}

void
main_configure_client(struct env *env)
{
	struct idm	*idm;
	struct imsgbuf	*ibuf = env->sc_ibuf;

	imsg_compose(ibuf, IMSG_CONF_START, 0, 0, env, sizeof(*env));
	TAILQ_FOREACH(idm, &env->sc_idms, idm_entry) {
		imsg_compose(ibuf, IMSG_CONF_IDM, 0, 0, idm, sizeof(*idm));
	}
	imsg_compose(ibuf, IMSG_CONF_END, 0, 0, NULL, 0);
}

void
main_init_timer(int fd, short event, void *p)
{
	struct env	*env = p;

	main_configure_client(env);
}

void
purge_config(struct env *env)
{
	struct idm	*idm;

	while ((idm = TAILQ_FIRST(&env->sc_idms)) != NULL) {
		TAILQ_REMOVE(&env->sc_idms, idm, idm_entry);
		free(idm);
	}
}

int
main(int argc, char *argv[])
{
	int		 c;
	int		 debug;
	struct passwd	*pw;
	struct env	 env;
	struct event	 ev_sigint;
	struct event	 ev_sigterm;
	struct event	 ev_sigchld;
	struct event	 ev_sighup;
	struct event	 ev_timer;
	struct timeval	 tv;

	debug = 0;
	ypldap_process = PROC_MAIN;

	log_init(1);

	while ((c = getopt(argc, argv, "dD:nf:v")) != -1) {
		switch (c) {
		case 'd':
			debug = 2;
			break;
		case 'D':
			if (cmdline_symset(optarg) < 0)
				log_warnx("could not parse macro definition %s",
				    optarg);
			break;
		case 'n':
			debug = 2;
			opts |= YPLDAP_OPT_NOACTION;
			break;
		case 'f':
			conffile = optarg;
			break;
		case 'v':
			opts |= YPLDAP_OPT_VERBOSE;
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc)
		usage();

	RB_INIT(&env.sc_user_uids);
	RB_INIT(&env.sc_group_gids);

	if (parse_config(&env, conffile, opts))
		exit(1);
	if (opts & YPLDAP_OPT_NOACTION) {
		fprintf(stderr, "configuration OK\n");
		exit(0);
	}

	if (geteuid())
		errx(1, "need root privileges");

	log_init(debug);

	if (!debug) {
		if (daemon(1, 0) == -1)
			err(1, "failed to daemonize");
	}

	log_info("startup%s", (debug > 1)?" [debug mode]":"");

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC,
	    pipe_main2client) == -1)
		fatal("socketpair");

	set_nonblock(pipe_main2client[0]);
	set_nonblock(pipe_main2client[1]);

	client_pid = ldapclient(pipe_main2client);

	setproctitle("parent");
	event_init();

	signal_set(&ev_sigint, SIGINT, main_sig_handler, &env);
	signal_set(&ev_sigterm, SIGTERM, main_sig_handler, &env);
	signal_set(&ev_sighup, SIGHUP, main_sig_handler, &env);
	signal_set(&ev_sigchld, SIGCHLD, main_sig_handler, &env);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal_add(&ev_sighup, NULL);
	signal_add(&ev_sigchld, NULL);

	close(pipe_main2client[1]);
	if ((env.sc_ibuf = calloc(1, sizeof(*env.sc_ibuf))) == NULL)
		fatal(NULL);
	imsg_init(env.sc_ibuf, pipe_main2client[0], main_dispatch_client);

	env.sc_ibuf->events = EV_READ;
	env.sc_ibuf->data = &env;
	event_set(&env.sc_ibuf->ev, env.sc_ibuf->fd, env.sc_ibuf->events,
	     env.sc_ibuf->handler, &env);
	event_add(&env.sc_ibuf->ev, NULL);

	yp_init(&env);

	if ((pw = getpwnam(YPLDAP_USER)) == NULL)
		fatal("getpwnam");

#ifndef DEBUG
	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("cannot drop privileges");
#else
#warning disabling privilege revocation in debug mode
#endif

	bzero(&tv, sizeof(tv));
	evtimer_set(&ev_timer, main_init_timer, &env);
	evtimer_add(&ev_timer, &tv);

	yp_enable_events();
	event_dispatch();
	main_shutdown();

	return (0);
}

void
imsg_event_add(struct imsgbuf *ibuf)
{
	struct env	*env = ibuf->data;

	ibuf->events = EV_READ;
	if (ibuf->w.queued)
		ibuf->events |= EV_WRITE;

	event_del(&ibuf->ev);
	event_set(&ibuf->ev, ibuf->fd, ibuf->events, ibuf->handler, env);
	event_add(&ibuf->ev, NULL);
}

void
set_nonblock(int fd)
{
	int	flags;

	if ((flags = fcntl(fd, F_GETFL, 0)) == -1)
		fatal("fcntl F_GETFL");

	flags |= O_NONBLOCK;

	if ((flags = fcntl(fd, F_SETFL, flags)) == -1)
		fatal("fcntl F_SETFL");
}
