/*	$OpenBSD: ifstated.c,v 1.1 2004/01/23 21:34:30 mcbride Exp $	*/

/*
 * Copyright (c) 2004 Marco Pfatschbacher <mpf@openbsd.org>
 * Copyright (c) 2004 Ryan McBride <mcbride@openbsd.org>
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

/*
 * ifstated listens to link_state transitions on interfaces
 * and executes predefined commands.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <err.h>
#include <event.h>
#include <util.h>
#include <unistd.h>
#include <syslog.h>
#include <stdarg.h>
#include <ifaddrs.h>

#include "ifstated.h"


struct	 ifsd_config conf;

int	 opt_debug = 0;
int	 opt_inhibit = 0; 
char 	*configfile = "/etc/ifstated.conf";

void	startup_handler(int, short, void *);
void	sighup_handler(int, short, void *);
void	load_config(void);
void	sigchld_handler(int, short, void *);
void	rt_msg_handler(int, short, void *);
void	external_handler(int, short, void *);
void	external_async_exec(struct ifsd_external *);
void	check_external_status(struct ifsd_state *);
void	external_evtimer_setup(struct ifsd_state *, int);
int	scan_ifstate(int, int, struct ifsd_state *);
void	fetch_state(void);
void	usage(void);
void	doconfig(const char*);
void	adjust_expressions(struct ifsd_expression_list *, int);
void	eval_state(struct ifsd_state *);
void	state_change(void);
void	do_action(struct ifsd_action *);
void	clear_config(struct ifsd_config *);
void	remove_action(struct ifsd_action *, struct ifsd_state *);
void	remove_expression(struct ifsd_expression *, struct ifsd_state *);

#define LOG(l,s,a) do {					\
	if (l <= conf.loglevel) {			\
		if (opt_debug)				\
			printf("ifstated: " s , a );	\
		else					\
			syslog(LOG_DAEMON, s , a);	\
	}						\
} while(0)

void
usage(void)
{
	fprintf(stderr, "usage: ifstated [-hdi] [-f config]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct event startup_ev, sighup_ev, sigchld_ev, rt_msg_ev;
	int rt_fd, ch;
	struct timeval tv;
	
	while ((ch = getopt(argc, argv, "dD:f:hi")) != -1) {
		switch (ch) {
		case 'd':
			opt_debug = 1;
			break;
		case 'D':
			if (cmdline_symset(optarg) < 0)
				errx(1, "could not parse macro definition %s",
				    optarg); 
			break;
		case 'f':
			configfile = optarg;
			break;
		case 'h':
			usage();
			break;
		case 'i':
			opt_inhibit = 1;
			break;
		case 'v':
			if (conf.opts & IFSD_OPT_VERBOSE)
				conf.opts |= IFSD_OPT_VERBOSE2;
			conf.opts |= IFSD_OPT_VERBOSE;
			break;
		default:
			usage();
		}
	}

	event_init();

	if (parse_config(configfile, &conf) != 0)
		errx(1, NULL);


	if (!opt_debug) {
		daemon(0, 0);
		setproctitle(NULL);
	}

	if ((rt_fd = socket(PF_ROUTE, SOCK_RAW, 0)) < 0)
                errx(1, "no routing socket");

	event_set(&rt_msg_ev, rt_fd, EV_READ|EV_PERSIST,
	    rt_msg_handler, &rt_msg_ev);
	event_add(&rt_msg_ev, NULL);

	signal_set(&sighup_ev, SIGHUP, sighup_handler, &sighup_ev);
	signal_add(&sighup_ev, NULL);

	signal_set(&sigchld_ev, SIGCHLD, sigchld_handler, &sigchld_ev);
	signal_add(&sigchld_ev, NULL);

	/* Loading the config needs to happen in the event loop */
	tv.tv_usec = 0;
	tv.tv_sec = 0;
	evtimer_set(&startup_ev, startup_handler, &startup_ev);
	evtimer_add(&startup_ev, &tv);

	event_loop(0);
	exit(0);
}

void
startup_handler(int fd, short event, void *arg)
{
	LOG(IFSD_LOG_NORMAL, "%s\n", "started");
	load_config();
}

void
sighup_handler(int fd, short event, void *arg)
{
	LOG(IFSD_LOG_NORMAL, "%s\n", "reloading config");
	clear_config(&conf);
	load_config();
}

void
load_config(void)
{
	parse_config(configfile, &conf);
	conf.always.entered = time(NULL);
	fetch_state();
	eval_state(&conf.always);
	if (conf.curstate != NULL) {
		LOG(IFSD_LOG_NORMAL,
		    "initial state: %s\n", conf.curstate->name);
		conf.curstate->entered = time(NULL);
		eval_state(conf.curstate);
	}
	external_evtimer_setup(&conf.always, IFSD_EVTIMER_ADD);
}

void
rt_msg_handler(int fd, short event, void *arg)
{       
	struct if_msghdr *ifm;
	char msg[2048];
	struct rt_msghdr *rtm = (struct rt_msghdr *)&msg;
	int len;
        
	len = read(fd, msg, sizeof(msg)); 

	/* XXX ignore errors? */
	if (len < sizeof(struct rt_msghdr))
		return;

	if (rtm->rtm_version != RTM_VERSION)
		return;

	if (rtm->rtm_type != RTM_IFINFO)
		return;

	ifm = (struct if_msghdr *)rtm;

	if (scan_ifstate(ifm->ifm_index, ifm->ifm_data.ifi_link_state,
	    &conf.always))
		eval_state(&conf.always);
	if ((conf.curstate != NULL) && scan_ifstate(ifm->ifm_index,
	    ifm->ifm_data.ifi_link_state, conf.curstate))
		eval_state(conf.curstate);
}

void
sigchld_handler(int fd, short event, void *arg)
{
	check_external_status(&conf.always);
	if (conf.curstate != NULL)
		check_external_status(conf.curstate);
}

void
external_handler(int fd, short event, void *arg)
{
	struct ifsd_external *external = (struct ifsd_external *)arg;
	struct timeval tv;

	/* re-schedule */
	tv.tv_usec = 0;
	tv.tv_sec = external->frequency;
	evtimer_set(&external->ev, external_handler, external);
	evtimer_add(&external->ev, &tv);
	
	/* execute */
	external_async_exec(external);
}

void
external_async_exec(struct ifsd_external *external)
{
	pid_t pid;
	char *argp[] = {"sh", "-c", NULL, NULL};

	if (external->pid > 0) {
		LOG(IFSD_LOG_NORMAL, 
		    "previous command %s still running, killing it\n",
		    external->command);
		kill(external->pid, SIGKILL);
		external->pid = 0;
	}

	argp[2] = external->command;
	LOG(IFSD_LOG_VERBOSE, "running %s\n", external->command);
	pid = fork();
	if (pid < 0) {
		LOG(IFSD_LOG_QUIET, "%s", "fork error");
	} else if (pid == 0) {
		execv("/bin/sh", argp);
		_exit(1);
		/* NOTREACHED */
	} else {
		external->pid = pid;
	}
}

void
check_external_status(struct ifsd_state *state)
{
	struct ifsd_external *external, *end = NULL;
	struct ifsd_expression_list expressions;	
	int status, s, changed = 0;
	
	TAILQ_INIT(&expressions);

	/* Do this manually; change ordering so the oldest is first */
	external = TAILQ_FIRST(&state->external_tests);
	while (external != NULL && external != end) {
		struct ifsd_external *newexternal;

		newexternal = TAILQ_NEXT(external, entries);

		if (external->pid <= 0)
			goto loop;

		if (wait4(external->pid, &s, WNOHANG, NULL) == 0)
			goto loop;

		external->pid = 0;
		if (end == NULL)
			end = external;
		if (WIFEXITED(s))
			status = WEXITSTATUS(s);
		else {
			LOG(IFSD_LOG_QUIET,
			    "%s exited abnormally", external->command);
			goto loop;
		}

		if (external->prevstatus != status &&
		    (external->prevstatus != -1 || !opt_inhibit)) {
			struct ifsd_expression *expression;

			changed = 1;
			TAILQ_FOREACH(expression,
			    &external->expressions, entries) {
				TAILQ_INSERT_TAIL(&expressions,
				    expression, eval);
				if (status == 0)
					expression->truth = 1;
				else
					expression->truth = 0;
			}
		}
		external->lastexec = time(NULL);
		TAILQ_REMOVE(&state->external_tests, external, entries);
		TAILQ_INSERT_TAIL(&state->external_tests, external, entries);
		external->prevstatus = status;
loop:
		external = newexternal;
	}

	if (changed) {
		adjust_expressions(&expressions, conf.maxdepth);
		eval_state(state);
	}
}

void
external_evtimer_setup(struct ifsd_state *state, int action)
{
	struct ifsd_external *external;

	if (state != NULL) {
		switch (action) {
		case IFSD_EVTIMER_ADD:
			TAILQ_FOREACH(external,
			    &state->external_tests, entries) {
				struct timeval tv;

				/* run it once right away */
				external_async_exec(external);

				/* schedule it for later */
				tv.tv_usec = 0;
				tv.tv_sec = external->frequency;
				evtimer_set(&external->ev, external_handler,
				    external);
				evtimer_add(&external->ev, &tv);
			}
			break;
		case IFSD_EVTIMER_DEL:
			TAILQ_FOREACH(external,
			   &state->external_tests, entries) {
				if (external->pid > 0) {
					kill(external->pid, SIGKILL);
					external->pid = 0;
				}
				evtimer_del(&external->ev);
			}
			break;
		}
	}
}

int
scan_ifstate(int ifindex, int s, struct ifsd_state *state)
{
	struct ifsd_ifstate *ifstate;
	struct ifsd_expression_list expressions;	
	int changed = 0;

	TAILQ_INIT(&expressions);

	TAILQ_FOREACH(ifstate, &state->interface_states, entries) {
		if (ifstate->ifindex == ifindex) {
			
			if (ifstate->prevstate != s &&
			    (ifstate->prevstate != -1 || !opt_inhibit)) {
				struct ifsd_expression *expression;
				int truth;

				if (ifstate->ifstate == s)
					truth = 1;
				else
					truth = 0;

				TAILQ_FOREACH(expression,
				    &ifstate->expressions, entries) {
					expression->truth = truth;
					TAILQ_INSERT_TAIL(&expressions,
					    expression, eval);
					changed = 1;
				}
				ifstate->prevstate = s;
			}
		}
	}

	if (changed)
		adjust_expressions(&expressions, conf.maxdepth);
	return (changed);
}

/*
 * Do a bottom-up ajustment of the expression tree's truth value,
 * level-by-level to ensure that each expression's subexpressions have been
 * evaluated.
 */
void
adjust_expressions(struct ifsd_expression_list *expressions, int depth)
{
	struct ifsd_expression_list nexpressions;
	struct ifsd_expression *expression;

	TAILQ_INIT(&nexpressions);
	while ((expression = TAILQ_FIRST(expressions)) != NULL) {
		TAILQ_REMOVE(expressions, expression, eval);
		if (expression->depth == depth) {
			struct ifsd_expression *te;

			switch (expression->type) {
			case IFSD_OPER_AND:
				if (expression->left->truth &&
				    expression->right->truth)
					expression->truth = 1;
				else
					expression->truth = 0;
				break;
			case IFSD_OPER_OR:
				if (expression->left->truth ||
				    expression->right->truth)
					expression->truth = 1;
				else
					expression->truth = 0;
				break;
			case IFSD_OPER_NOT:
				if (expression->right->truth)
					expression->truth = 0;
				else
					expression->truth = 1;
				break;
			default:
				break;
			}
			if (expression->parent != NULL){ 
				if (TAILQ_EMPTY(&nexpressions))
				te = NULL;
				TAILQ_FOREACH(te, &nexpressions, eval)
					if (expression->parent == te)
						break;
				if (te == NULL)
					TAILQ_INSERT_TAIL(&nexpressions,
					    expression->parent, eval);
			}
		} else
			TAILQ_INSERT_TAIL(&nexpressions, expression, eval);
	}
	if (depth > 0)
		adjust_expressions(&nexpressions, depth - 1);
}

void
eval_state(struct ifsd_state *state)
{
	struct ifsd_external *external = TAILQ_FIRST(&state->external_tests);
	if (external == NULL || external->lastexec >= state->entered) {
		do_action(state->always);
		state_change();
	}
}

/*
 *If a previous action included a state change, process it.
 */
void
state_change(void)
{
	if (conf.nextstate != NULL && conf.curstate != conf.nextstate) {
		LOG(IFSD_LOG_NORMAL, "changing state to %s\n",
		    conf.nextstate->name);
		evtimer_del(&conf.curstate->ev);
		if (conf.curstate != NULL)
			external_evtimer_setup(conf.curstate, IFSD_EVTIMER_DEL);
		conf.curstate = conf.nextstate;
		conf.nextstate = NULL;
		conf.curstate->entered = time(NULL);
		external_evtimer_setup(conf.curstate, IFSD_EVTIMER_ADD);
		fetch_state();
		do_action(conf.curstate->init);
		fetch_state();
	} 
}

/*
 * Run recursively through the tree of actions.
 */
void
do_action(struct ifsd_action *action)
{
	struct ifsd_action *subaction;

	switch (action->type) {
	case IFSD_ACTION_COMMAND:
		LOG(IFSD_LOG_NORMAL, "running %s\n", action->act.command);
		system(action->act.command);
		break;
	case IFSD_ACTION_CHANGESTATE:
		conf.nextstate = action->act.nextstate;
		break;
	case IFSD_ACTION_CONDITION:
		if ((action->act.c.expression != NULL &&
		    action->act.c.expression->truth) ||
		    action->act.c.expression == NULL) {
			TAILQ_FOREACH(subaction, &action->act.c.actions,
			    entries)
				do_action(subaction);
		}
		break;
	default:
		LOG(IFSD_LOG_DEBUG, "do_action: unknown action %d",
		    action->type);
		break;
	}
}

/*
 * Fetch the current link states.
 */
void
fetch_state(void)
{
	struct ifaddrs *ifap, *ifa;
	char *oname = NULL;
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	
	if (getifaddrs(&ifap) != 0)
                err(1, "getifaddrs");

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		struct ifreq ifr;
		struct if_data  ifrdat;

		if (oname && !strcmp(oname, ifa->ifa_name))
			continue;
		oname = ifa->ifa_name;

		strlcpy(ifr.ifr_name, ifa->ifa_name, sizeof(ifr.ifr_name));
		ifr.ifr_data = (caddr_t)&ifrdat;

		if (ioctl(sock, SIOCGIFDATA, (caddr_t)&ifr) == -1)
			continue;

		scan_ifstate(if_nametoindex(ifa->ifa_name),
		    ifrdat.ifi_link_state, &conf.always);
		if (conf.curstate != NULL)
			scan_ifstate(if_nametoindex(ifa->ifa_name),
			    ifrdat.ifi_link_state, conf.curstate);
	}
	close(sock);
}



/*
 * Clear the config.
 */
void
clear_config(struct ifsd_config *oconf)
{
	struct ifsd_state *state;

	external_evtimer_setup(&conf.always, IFSD_EVTIMER_DEL);
	if (conf.curstate != NULL)
		external_evtimer_setup(conf.curstate, IFSD_EVTIMER_DEL);
	while ((state = TAILQ_FIRST(&oconf->states)) != NULL) {
		TAILQ_REMOVE(&oconf->states, state, entries);
		remove_action(state->init, state);
		remove_action(state->always, state);
		free(state->name);
		free(state);
	}
	remove_action(oconf->always.init, &oconf->always);
	remove_action(oconf->always.always, &oconf->always);
}

void
remove_action(struct ifsd_action *action, struct ifsd_state *state)
{
	struct ifsd_action *subaction;

	if (action == NULL || state == NULL)
		return;

	switch (action->type) {
	case IFSD_ACTION_LOG:
		free(action->act.logmessage);
		break;
	case IFSD_ACTION_COMMAND:
		free(action->act.command);
		break;
	case IFSD_ACTION_CHANGESTATE:
		break;
	case IFSD_ACTION_CONDITION:
		if (action->act.c.expression != NULL)
			remove_expression(action->act.c.expression, state);
		while ((subaction =
		    TAILQ_FIRST(&action->act.c.actions)) != NULL) {
			TAILQ_REMOVE(&action->act.c.actions,
			    subaction, entries);
			remove_action(subaction, state);
		}
	}
	free(action);
}

void
remove_expression(struct ifsd_expression *expression,
    struct ifsd_state *state)
{
	switch (expression->type) {
	case IFSD_OPER_IFSTATE:
		TAILQ_REMOVE(&expression->u.ifstate->expressions, expression,
		    entries);
		if (--expression->u.ifstate->refcount == 0) {
			TAILQ_REMOVE(&state->interface_states,
			    expression->u.ifstate, entries);
			free(expression->u.ifstate);
		}
		break;
	case IFSD_OPER_EXTERNAL:
		TAILQ_REMOVE(&expression->u.external->expressions, expression,
		    entries);
		if (--expression->u.external->refcount == 0) {
			TAILQ_REMOVE(&state->external_tests,
			    expression->u.external, entries);
			free(expression->u.external->command);
			event_del(&expression->u.external->ev);
			free(expression->u.external);
		}
		break;
	default:
		if (expression->left != NULL)
			remove_expression(expression->left, state);
		if (expression->right != NULL)
			remove_expression(expression->right, state);
		break;
	}
	free(expression);
}
