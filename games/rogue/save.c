/*	$OpenBSD: save.c,v 1.9 2003/06/03 03:01:41 millert Exp $	*/
/*	$NetBSD: save.c,v 1.3 1995/04/22 10:28:21 cgd Exp $	*/

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
static char sccsid[] = "@(#)save.c	8.1 (Berkeley) 5/31/93";
#else
static const char rcsid[] = "$OpenBSD: save.c,v 1.9 2003/06/03 03:01:41 millert Exp $";
#endif
#endif /* not lint */

/*
 * save.c
 *
 * This source herein may be modified and/or distributed by anybody who
 * so desires, with the following restrictions:
 *    1.)  No portion of this notice shall be removed.
 *    2.)  Credit shall not be taken for the creation of this source.
 *    3.)  This code is not to be traded, sold, or used for personal
 *         gain or profit.
 *
 */

#include <sys/param.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include "rogue.h"

char *save_file = (char *) NULL;

static short write_failed = 0;

static void write_pack(object *, int);
static void read_pack(object *, int, boolean);
static void rw_dungeon(int, boolean);
static void rw_id(struct id *, int, int, boolean);
static void write_string(char *, int);
static void read_string(char *, size_t, int);
static void rw_rooms(int, boolean);
static void r_read(int, char *, size_t);
static void r_write(int, char *, size_t);
static boolean has_been_touched(struct rogue_time *, struct rogue_time *);


void
save_game()
{
	char fname[MAXPATHLEN];

	if (!get_input_line("file name?", save_file, fname, sizeof(fname),
	    "game not saved", 0, 1)) {
		return;
	}
	check_message();
	messagef(0, "%s", fname);
	save_into_file(fname);
}

void
save_into_file(sfile)
	const char *sfile;
{
	int fp;
	int file_id;
	char name_buffer[MAXPATHLEN];
	char *hptr;
	struct rogue_time rt_buf;

	if (sfile[0] == '~') {
		if ((hptr = md_getenv("HOME"))) {
			if (strlen(hptr) + strlen(sfile+1) < sizeof(name_buffer)) {
				(void) strlcpy(name_buffer, hptr,
					sizeof name_buffer);
				(void) strlcat(name_buffer, sfile+1,
					sizeof name_buffer);
				sfile = name_buffer;
			} else {
				messagef(0, "homedir is too long");
				return;
			}
		}
	}
	if (	((fp = open(sfile, O_WRONLY | O_CREAT | O_TRUNC, 0644)) == -1) ||
			((file_id = md_get_file_id(sfile)) == -1)) {
		messagef(0, "problem accessing the save file");
		if (fp != -1)
			close(fp);
		return;
	}
	md_ignore_signals();
	write_failed = 0;
	(void) xxx(1);
	r_write(fp, (char *) &detect_monster, sizeof(detect_monster));
	r_write(fp, (char *) &cur_level, sizeof(cur_level));
	r_write(fp, (char *) &max_level, sizeof(max_level));
	write_string(hunger_str, fp);
	write_string(login_name, fp);
	r_write(fp, (char *) &party_room, sizeof(party_room));
	write_pack(&level_monsters, fp);
	write_pack(&level_objects, fp);
	r_write(fp, (char *) &file_id, sizeof(file_id));
	rw_dungeon(fp, 1);
	r_write(fp, (char *) &foods, sizeof(foods));
	r_write(fp, (char *) &rogue, sizeof(fighter));
	write_pack(&rogue.pack, fp);
	rw_id(id_potions, fp, POTIONS, 1);
	rw_id(id_scrolls, fp, SCROLS, 1);
	rw_id(id_wands, fp, WANDS, 1);
	rw_id(id_rings, fp, RINGS, 1);
	r_write(fp, (char *) traps, (MAX_TRAPS * sizeof(trap)));
	r_write(fp, (char *) is_wood, (WANDS * sizeof(boolean)));
	r_write(fp, (char *) &cur_room, sizeof(cur_room));
	rw_rooms(fp, 1);
	r_write(fp, (char *) &being_held, sizeof(being_held));
	r_write(fp, (char *) &bear_trap, sizeof(bear_trap));
	r_write(fp, (char *) &halluc, sizeof(halluc));
	r_write(fp, (char *) &blind, sizeof(blind));
	r_write(fp, (char *) &confused, sizeof(confused));
	r_write(fp, (char *) &levitate, sizeof(levitate));
	r_write(fp, (char *) &haste_self, sizeof(haste_self));
	r_write(fp, (char *) &see_invisible, sizeof(see_invisible));
	r_write(fp, (char *) &detect_monster, sizeof(detect_monster));
	r_write(fp, (char *) &wizard, sizeof(wizard));
	r_write(fp, (char *) &score_only, sizeof(score_only));
	r_write(fp, (char *) &m_moves, sizeof(m_moves));
	md_gct(&rt_buf);
	rt_buf.second += 10;		/* allow for some processing time */
	r_write(fp, (char *) &rt_buf, sizeof(rt_buf));
	close(fp);

	if (write_failed) {
		(void) md_df(sfile);	/* delete file */
	} else {
		clean_up("");
	}
}

void
restore(fname)
	char *fname;
{
	int fp;
	struct rogue_time saved_time, mod_time;
	char buf[4];
	char tbuf[LOGIN_NAME_LEN];
	int new_file_id, saved_file_id;

	if (((new_file_id = md_get_file_id(fname)) == -1) ||
			((fp = open(fname, O_RDONLY, 0)) == NULL)) {
		clean_up("cannot open file");
		return;	/* NOT REACHED */
	}
	if (md_link_count(fname) > 1) {
		clean_up("file has link");
	}
	(void) xxx(1);
	r_read(fp, (char *) &detect_monster, sizeof(detect_monster));
	r_read(fp, (char *) &cur_level, sizeof(cur_level));
	r_read(fp, (char *) &max_level, sizeof(max_level));
	read_string(hunger_str, HUNGER_STR_LEN, fp);

	strlcpy(tbuf, login_name, sizeof(tbuf));
	read_string(login_name, LOGIN_NAME_LEN, fp);
	if (strcmp(tbuf, login_name)) {
		clean_up("you're not the original player");
	}

	r_read(fp, (char *) &party_room, sizeof(party_room));
	read_pack(&level_monsters, fp, 0);
	read_pack(&level_objects, fp, 0);
	r_read(fp, (char *) &saved_file_id, sizeof(saved_file_id));
	if (new_file_id != saved_file_id) {
		clean_up("sorry, saved game is not in the same file");
	}
	rw_dungeon(fp, 0);
	r_read(fp, (char *) &foods, sizeof(foods));
	r_read(fp, (char *) &rogue, sizeof(fighter));
	read_pack(&rogue.pack, fp, 1);
	rw_id(id_potions, fp, POTIONS, 0);
	rw_id(id_scrolls, fp, SCROLS, 0);
	rw_id(id_wands, fp, WANDS, 0);
	rw_id(id_rings, fp, RINGS, 0);
	r_read(fp, (char *) traps, (MAX_TRAPS * sizeof(trap)));
	r_read(fp, (char *) is_wood, (WANDS * sizeof(boolean)));
	r_read(fp, (char *) &cur_room, sizeof(cur_room));
	rw_rooms(fp, 0);
	r_read(fp, (char *) &being_held, sizeof(being_held));
	r_read(fp, (char *) &bear_trap, sizeof(bear_trap));
	r_read(fp, (char *) &halluc, sizeof(halluc));
	r_read(fp, (char *) &blind, sizeof(blind));
	r_read(fp, (char *) &confused, sizeof(confused));
	r_read(fp, (char *) &levitate, sizeof(levitate));
	r_read(fp, (char *) &haste_self, sizeof(haste_self));
	r_read(fp, (char *) &see_invisible, sizeof(see_invisible));
	r_read(fp, (char *) &detect_monster, sizeof(detect_monster));
	r_read(fp, (char *) &wizard, sizeof(wizard));
	r_read(fp, (char *) &score_only, sizeof(score_only));
	r_read(fp, (char *) &m_moves, sizeof(m_moves));
	r_read(fp, (char *) &saved_time, sizeof(saved_time));

	if (read(fp, buf, 1) > 0) {
		clear();
		clean_up("extra characters in file");
	}

	md_gfmt(fname, &mod_time);	/* get file modification time */

	if (has_been_touched(&saved_time, &mod_time)) {
		clear();
		clean_up("sorry, file has been touched");
	}
	if ((!wizard) && !md_df(fname)) {
		clean_up("cannot delete file");
	}
	msg_cleared = 0;
	ring_stats(0);
	close(fp);
}

static void
write_pack(pack, fp)
	object *pack;
	int fp;
{
	object t;

	while ((pack = pack->next_object)) {
		r_write(fp, (char *) pack, sizeof(object));
	}
	t.ichar = t.what_is = 0;
	r_write(fp, (char *) &t, sizeof(object));
}

static void
read_pack(pack, fp, is_rogue)
	object *pack;
	int fp;
	boolean is_rogue;
{
	object read_obj, *new_obj;

	for (;;) {
		r_read(fp, (char *) &read_obj, sizeof(object));
		if (read_obj.ichar == 0) {
			pack->next_object = (object *) 0;
			break;
		}
		new_obj = alloc_object();
		*new_obj = read_obj;
		if (is_rogue) {
			if (new_obj->in_use_flags & BEING_WORN) {
					do_wear(new_obj);
			} else if (new_obj->in_use_flags & BEING_WIELDED) {
					do_wield(new_obj);
			} else if (new_obj->in_use_flags & (ON_EITHER_HAND)) {
				do_put_on(new_obj,
					((new_obj->in_use_flags & ON_LEFT_HAND) ? 1 : 0));
			}
		}
		pack->next_object = new_obj;
		pack = new_obj;
	}
}

static void
rw_dungeon(fp, rw)
	int fp;
	boolean rw;
{
	short i, j;
	char buf[DCOLS];

	for (i = 0; i < DROWS; i++) {
		if (rw) {
			r_write(fp, (char *) dungeon[i], (DCOLS * sizeof(dungeon[0][0])));
			for (j = 0; j < DCOLS; j++) {
				buf[j] = mvinch(i, j);
			}
			r_write(fp, buf, DCOLS);
		} else {
			r_read(fp, (char *) dungeon[i], (DCOLS * sizeof(dungeon[0][0])));
			r_read(fp, buf, DCOLS);
			for (j = 0; j < DCOLS; j++) {
				mvaddch(i, j, buf[j]);
			}
		}
	}
}

static void
rw_id(id_table, fp, n, wr)
	struct id id_table[];
	int fp;
	int n;
	boolean wr;
{
	short i;

	for (i = 0; i < n; i++) {
		if (wr) {
			r_write(fp, (char *) &(id_table[i].value), sizeof(short));
			r_write(fp, (char *) &(id_table[i].id_status),
			    sizeof(unsigned short));
			write_string(id_table[i].title, fp);
		} else {
			r_read(fp, (char *) &(id_table[i].value), sizeof(short));
			r_read(fp, (char *) &(id_table[i].id_status),
			    sizeof(unsigned short));
			read_string(id_table[i].title, sizeof(id_table[i].title),
			    fp);
		}
	}
}

static void
write_string(s, fp)
	char *s;
	int fp;
{
	short n;

	n = strlen(s) + 1;
	xxxx(s, n);
	r_write(fp, (char *) &n, sizeof(short));
	r_write(fp, s, n);
}

static void
read_string(s, maxlen, fp)
	char *s;
	size_t maxlen;
	int fp;
{
	short n;

	r_read(fp, (char *) &n, sizeof(short));
	if (n <= 0 || (size_t)(unsigned short)n > maxlen)
		clean_up("saved game is corrupt");
	r_read(fp, s, n);
	xxxx(s, n);
	/* ensure NUL termination */
	s[n - 1] = '\0';
}

static void
rw_rooms(fp, rw)
	int fp;
	boolean rw;
{
	short i;

	for (i = 0; i < MAXROOMS; i++) {
		rw ? r_write(fp, (char *) (rooms + i), sizeof(room)) :
			r_read(fp, (char *) (rooms + i), sizeof(room));
	}
}

static void
r_read(fp, buf, n)
	int fp;
	char *buf;
	size_t n;
{
	if (read(fp, buf, n) != n) {
		clean_up("read() failed, don't know why");
	}
}

static void
r_write(fp, buf, n)
	int fp;
	char *buf;
	size_t n;
{
	if (!write_failed) {
		if (write(fp, buf, n) != n) {
			messagef(0, "write() failed, don't know why");
			beep();
			write_failed = 1;
		}
	}
}

static boolean
has_been_touched(saved_time, mod_time)
	struct rogue_time *saved_time, *mod_time;
{
	if (saved_time->year < mod_time->year) {
		return(1);
	} else if (saved_time->year > mod_time->year) {
		return(0);
	}
	if (saved_time->month < mod_time->month) {
		return(1);
	} else if (saved_time->month > mod_time->month) {
		return(0);
	}
	if (saved_time->day < mod_time->day) {
		return(1);
	} else if (saved_time->day > mod_time->day) {
		return(0);
	}
	if (saved_time->hour < mod_time->hour) {
		return(1);
	} else if (saved_time->hour > mod_time->hour) {
		return(0);
	}
	if (saved_time->minute < mod_time->minute) {
		return(1);
	} else if (saved_time->minute > mod_time->minute) {
		return(0);
	}
	if (saved_time->second < mod_time->second) {
		return(1);
	}
	return(0);
}
