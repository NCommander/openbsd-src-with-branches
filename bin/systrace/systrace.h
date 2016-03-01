/*	$OpenBSD: systrace.h,v 1.27 2006/07/02 12:34:15 sturm Exp $	*/
/*
 * Copyright 2002 Niels Provos <provos@citi.umich.edu>
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

#ifndef _SYSTRACE_H_
#define _SYSTRACE_H_
#include <sys/queue.h>

#define _PATH_XSYSTRACE	"/usr/X11R6/bin/xsystrace"

enum logicop { LOGIC_AND, LOGIC_OR, LOGIC_NOT, LOGIC_SINGLE };

struct logic {
	enum logicop op;
	struct logic *left;
	struct logic *right;
	char *type;
	int typeoff;
	int flags;
	void *filterdata;
	size_t filterlen;
	int (*filter_match)(struct intercept_translate *, struct logic *);
	void *filterarg;
};

#define LOGIC_NEEDEXPAND	0x01

struct filter {
	TAILQ_ENTRY(filter) next;
	TAILQ_ENTRY(filter) policy_next;

	char *rule;
	char name[32];
	char emulation[16];
	struct logic *logicroot;
	short match_action;
	int match_error;
	int match_flags;
	int match_count;	/* Number of times this filter matched */

	struct predicate {
#define PREDIC_UID	0x01
#define PREDIC_GID	0x02
#define PREDIC_NEGATIVE	0x10
#define PREDIC_LESSER	0x20
#define PREDIC_GREATER	0x30
#define PREDIC_MASK	0x30
		int p_flags;
		uid_t p_uid;
		gid_t p_gid;
	} match_predicate;

	struct elevate elevate;
};

TAILQ_HEAD(filterq, filter);

struct policy_syscall {
	SPLAY_ENTRY(policy_syscall) node;

	char name[64];
	char emulation[16];

	struct filterq flq;
};

struct policy {
	SPLAY_ENTRY(policy) node;
	SPLAY_ENTRY(policy) nrnode;

	const char *name;
	char emulation[16];

	struct timespec ts_last;	/* last time we read the file */

	SPLAY_HEAD(syscalltree, policy_syscall) pflqs;

	int policynr;			/* in-kernel policy number */
	short kerneltable[INTERCEPT_MAXSYSCALLNR];
	int flags;

	struct filterq filters;
	int nfilters;			/* nr of installed policy statements */
	struct filterq prefilters;	/* filters we need to install*/
};

struct template {
	TAILQ_ENTRY(template) next;

	char *filename;
	char *name;
	char *description;

	char *emulation;
};

TAILQ_HEAD(tmplqueue, template);

#define POLICY_PATH		"/etc/systrace"

#define POLICY_UNSUPERVISED	0x01	/* Auto-Pilot */
#define POLICY_DETACHED		0x02	/* Ignore this program */
#define POLICY_CHANGED		0x04

#define PROCESS_INHERIT_POLICY	0x01	/* Process inherits policy */
#define PROCESS_DETACH		0x02	/* Process gets detached */
#define SYSCALL_LOG		0x04	/* Log this system call */
#define PROCESS_PROMPT		0x08	/* Prompt but nothing else */

#define SYSTRACE_UPDATETIME	30	/* update policies every 30 seconds */

void systrace_parameters(void);
int systrace_initpolicy(char *, char *);
void systrace_setupdir(char *);
struct template *systrace_readtemplate(char *, struct policy *,
    struct template *);
void systrace_initcb(void);
struct policy *systrace_newpolicy(const char *, const char *);
void systrace_cleanpolicy(struct policy *);
void systrace_freepolicy(struct policy *);
int systrace_newpolicynr(int, struct policy *);
int systrace_modifypolicy(int, int, const char *, short);
struct policy *systrace_findpolicy(const char *);
struct policy *systrace_findpolicy_wildcard(const char *);
struct policy *systrace_findpolnr(int);
int systrace_dumppolicies(int);
int systrace_updatepolicies(int);
struct policy *systrace_readpolicy(const char *);
int systrace_addpolicy(const char *);
int systrace_updatepolicy(int fd, struct policy *policy);
struct filterq *systrace_policyflq(struct policy *, const char *, const char *);
char *systrace_getpolicyname(const char *);

int systrace_error_translate(char *);

#define SYSTRACE_MAXALIAS	10

struct systrace_alias {
	SPLAY_ENTRY(systrace_alias) node;
	TAILQ_ENTRY(systrace_alias) next;

	char name[64];
	char emulation[16];

	char aname[64];
	char aemul[16];

	struct intercept_translate *arguments[SYSTRACE_MAXALIAS];
	int nargs;

	struct systrace_revalias *reverse;
};

int systrace_initalias(void);
struct systrace_alias *systrace_new_alias(const char *, const char *, char *, char *);
void systrace_switch_alias(const char *, const char *, char *, char *);
struct systrace_alias *systrace_find_alias(const char *, const char *);
void systrace_alias_add_trans(struct systrace_alias *,
    struct intercept_translate *);

struct systrace_revalias {
	SPLAY_ENTRY(systrace_revalias) node;

	char name[64];
	char emulation[16];

	TAILQ_HEAD(revaliasq, systrace_alias) revl;
};

struct systrace_revalias *systrace_reverse(const char *, const char *);
struct systrace_revalias *systrace_find_reverse(const char *, const char *);

short filter_evaluate(struct intercept_tlq *, struct filterq *,
    struct intercept_pid *);
short filter_ask(int, struct intercept_tlq *, struct filterq *, int,
    const char *, const char *, char *, short *, struct intercept_pid *);
void filter_free(struct filter *);
void filter_modifypolicy(int, int, const char *, const char *, short);

int filter_predicate(struct intercept_pid *, struct predicate *);
int filter_parse_simple(char *, short *, short *);
int filter_parse(char *, struct filter **);
int filter_prepolicy(int, struct policy *);
char *filter_expand(char *);
char *filter_dynamicexpand(struct intercept_pid *, char *);
int filter_needexpand(char *);

void cradle_start(char *, char *, char *);

int parse_filter(char *, struct filter **);

char *uid_to_name(uid_t);

char *strrpl(char *, size_t, char *, char *);

void make_output(char *, size_t, const char *, pid_t, pid_t, int,
    const char *, int, const char *, const char *, int, struct intercept_tlq *,
    struct intercept_replace *);
short trans_cb(int, pid_t, int, const char *, int, const char *, void *,
    int, struct intercept_replace *, struct intercept_tlq *, void *);
short gen_cb(int, pid_t, int, const char *, int, const char *, void *,
    int, void *);
void execres_cb(int, pid_t, int, const char *, const char *, void *);
void policyfree_cb(int, void *);

extern struct intercept_translate ic_oflags;
extern struct intercept_translate ic_modeflags;
extern struct intercept_translate ic_fdt;
extern struct intercept_translate ic_uidt;
extern struct intercept_translate ic_uname;
extern struct intercept_translate ic_gidt;
extern struct intercept_translate ic_trargv;
extern struct intercept_translate ic_sockdom;
extern struct intercept_translate ic_socktype;
extern struct intercept_translate ic_pidname;
extern struct intercept_translate ic_signame;
extern struct intercept_translate ic_fcntlcmd;
extern struct intercept_translate ic_memprot;
extern struct intercept_translate ic_fileflags;

int requestor_start(char *, int);

#endif /* _SYSTRACE_H_ */
