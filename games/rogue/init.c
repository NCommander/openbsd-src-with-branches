/*	$OpenBSD: init.c,v 1.9 2004/01/21 19:12:13 espie Exp $	*/
/*	$NetBSD: init.c,v 1.4 1995/04/28 23:49:19 mycroft Exp $	*/

/*
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Timothy C. Stoehr.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)init.c	8.1 (Berkeley) 5/31/93";
#else
static const char rcsid[] = "$OpenBSD: init.c,v 1.9 2004/01/21 19:12:13 espie Exp $";
#endif
#endif /* not lint */

/*
 * init.c
 *
 * This source herein may be modified and/or distributed by anybody who
 * so desires, with the following restrictions:
 *    1.)  No portion of this notice shall be removed.
 *    2.)  Credit shall not be taken for the creation of this source.
 *    3.)  This code is not to be traded, sold, or used for personal
 *         gain or profit.
 *
 */

#include <err.h>
#include "rogue.h"

char login_name[LOGIN_NAME_LEN];
char *nick_name = NULL;
char *rest_file = 0;
boolean cant_int = 0;
boolean did_int = 0;
boolean score_only;
boolean init_curses = 0;
boolean save_is_interactive = 1;
boolean ask_quit = 1;
boolean no_skull = 0;
boolean passgo = 0;
const char *error_file = "rogue.esave";
const char *byebye_string = "Okay, bye bye!";
gid_t gid, egid;

int
init(argc, argv)
	int argc;
	char *argv[];
{
	char *pn;
	int seed;

	gid = getgid();
	egid = getegid();
	setegid(gid);
	pn = md_gln();
	if ((!pn) || (strlen(pn) >= LOGIN_NAME_LEN))
		clean_up("Hey!  Who are you?");
	(void) strlcpy(login_name, pn, sizeof(login_name));
	/* ':' is reserved.  If it's in a login_name, we're on a weird
	 * system... */
	while ((pn = strchr(login_name, ':')) != NULL)
		*pn = ';';

	do_args(argc, argv);
	do_opts();

	if (!score_only && !rest_file) {
		printf("Hello %s, just a moment while I dig the dungeon...",
			nick_name);
		fflush(stdout);
	}

	initscr();
	init_curses = 1;
	if ((LINES < DROWS) || (COLS < DCOLS)) {
		clean_up("must be played on 24 x 80 screen (or larger)");
	}
	start_window();

	md_heed_signals();

	if (score_only) {
		put_scores((object *) NULL, 0);
	}
	seed = md_gseed();
	(void) srrandom(seed);
	if (rest_file) {
		restore(rest_file);
		return(1);
	}
	mix_colors();
	get_wand_and_ring_materials();
	make_scroll_titles();

	level_objects.next_object = (object *) 0;
	level_monsters.next_monster = (object *) 0;
	player_init();
	ring_stats(0);
	return(0);
}

void
player_init()
{
	object *obj;

	rogue.pack.next_object = (object *) 0;

	obj = alloc_object();
	get_food(obj, 1);
	(void) add_to_pack(obj, &rogue.pack, 1);

	obj = alloc_object();		/* initial armor */
	obj->what_is = ARMOR;
	obj->which_kind = RINGMAIL;
	obj->class = RINGMAIL+2;
	obj->is_protected = 0;
	obj->d_enchant = 1;
	(void) add_to_pack(obj, &rogue.pack, 1);
	do_wear(obj);

	obj = alloc_object();		/* initial weapons */
	obj->what_is = WEAPON;
	obj->which_kind = MACE;
	obj->damage = "2d3";
	obj->hit_enchant = obj->d_enchant = 1;
	obj->identified = 1;
	(void) add_to_pack(obj, &rogue.pack, 1);
	do_wield(obj);

	obj = alloc_object();
	obj->what_is = WEAPON;
	obj->which_kind = BOW;
	obj->damage = "1d2";
	obj->hit_enchant = 1;
	obj->d_enchant = 0;
	obj->identified = 1;
	(void) add_to_pack(obj, &rogue.pack, 1);

	obj = alloc_object();
	obj->what_is = WEAPON;
	obj->which_kind = ARROW;
	obj->quantity = get_rand(25, 35);
	obj->damage = "1d2";
	obj->hit_enchant = 0;
	obj->d_enchant = 0;
	obj->identified = 1;
	(void) add_to_pack(obj, &rogue.pack, 1);
}

void
clean_up(estr)
	const char *estr;
{
	if (save_is_interactive) {
		if (init_curses) {
			move(DROWS-1, 0);
			refresh();
			stop_window();
		}
		printf("\n%s\n", estr);
	}
	md_exit(0);
}

void
start_window()
{
	cbreak();
	noecho();
#ifndef BAD_NONL
	nonl();
#endif
}

void
stop_window()
{
	endwin();
}

void
byebye(dummy)
	int dummy;
{
	md_ignore_signals();
	if (ask_quit) {
		quit(1);
	} else {
		clean_up(byebye_string);
	}
	md_heed_signals();
}

void
onintr(dummy)
	int dummy;
{
	md_ignore_signals();
	if (cant_int) {
		did_int = 1;
	} else {
		check_message();
		messagef(1, "interrupt");
	}
	md_heed_signals();
}

void
error_save(dummy)
	int dummy;
{
	save_is_interactive = 0;
	save_into_file(error_file);
	clean_up("");
}

void
do_args(argc, argv)
	int argc;
	char *argv[];
{
	int i, j;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			for (j = 1; argv[i][j]; j++) {
				switch(argv[i][j]) {
				case 's':
					score_only = 1;
					break;
				}
			}
		} else {
			rest_file = argv[i];
		}
	}
}

void
do_opts()
{
	char *eptr;

	if ((eptr = md_getenv("ROGUEOPTS"))) {
		for (;;) {
			while ((*eptr) == ' ') {
				eptr++;
			}
			if (!(*eptr))
				break;
			if (!strncmp(eptr, "fruit=", 6)) {
				eptr += 6;
				env_get_value(&fruit, eptr, 1);
			} else if (!strncmp(eptr, "file=", 5)) {
				eptr += 5;
				env_get_value(&save_file, eptr, 0);
			} else if (!strncmp(eptr, "jump", 4)) {
				jump = 1;
			} else if (!strncmp(eptr, "name=", 5)) {
				eptr += 5;
				env_get_value(&nick_name, eptr, 0);
			} else if (!strncmp(eptr, "noaskquit", 9)) {
				ask_quit = 0;
			} else if (!strncmp(eptr, "noskull", 5) ||
					!strncmp(eptr,"notomb", 6)) {
				no_skull = 1;
			} else if (!strncmp(eptr, "passgo", 5)) {
				passgo = 1;
			}
			while ((*eptr) && (*eptr != ',')) {
				eptr++;
			}
			if (!(*(eptr++))) {
				break;
			}
		}
	}
	/* If some strings have not been set through ROGUEOPTS, assign defaults
	 * to them so that the options editor has data to work with.
	 */
	init_str(&nick_name, login_name);
	init_str(&save_file, "rogue.save");
	init_str(&fruit, "slime-mold");
}

void
env_get_value(s, e, add_blank)
	char **s, *e;
	boolean add_blank;
{
	short i = 0;
	char *t;

	t = e;

	while ((*e) && (*e != ',')) {
		e++;
		if (++i >= MAX_OPT_LEN)
			break;
	}
	/* room.c:edit_opts() depends on length MAX_OPT_LEN + 2 */
	if (!(*s = md_malloc(MAX_OPT_LEN + 2)))
		clean_up("out of memory");
	(void) strlcpy(*s, t, MAX_OPT_LEN + 2);
	if (add_blank) {
		(*s)[i++] = ' ';
	}
	(*s)[i] = '\0';
}

void
init_str(str, dflt)
	char **str;
	const char *dflt;
{
	if (!(*str)) {
		/* room.c:edit_opts() depends on length MAX_OPT_LEN + 2 */
		*str = md_malloc(MAX_OPT_LEN + 2);
		if (*str == NULL)
			clean_up("out of memory");
		(void) strlcpy(*str, dflt, MAX_OPT_LEN + 2);
	}
}
