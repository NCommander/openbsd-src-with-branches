/* $OpenBSD$ */
/*
 * Copyright (c) 2002 Vincent Labrecque <vincent@openbsd.org>
 *							 All rights reserved.
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

#include "def.h"
#include "kbd.h"

#include <sys/queue.h>

#define MAX_LIST_RECORDS	32
#define MAX_FREE_RECORDS	32

/*
 * Local variables
 */
static struct undo_list	undo_list;
static int		undo_list_num;
static struct undo_list undo_free;
static int		undo_free_num;

/*
 * Global variables
 */
/*
 * undo_disable_flag -
 *
 * Stop doing undo (useful when we know are
 * going to deal with huge deletion/insertions
 * that we don't plan to undo)
 */
int undo_disable_flag;
int undoaction;			/* Are we called indirectly from undo()? */

/*
 * Local functions
 */
static int find_offset(LINE *, int);
static int find_linep(int, LINE **, int *);
static struct undo_rec *new_undo_record(void);
static void free_undo_record(struct undo_rec *);
static int drop_oldest_undo_record(void);

static int
find_offset(LINE *lp, int off)
{
	int count = 0;
	LINE *p;

	for (p = curwp->w_linep; p != lp; p = lforw(p)) {
		if (count != 0) {
			if (p == curbp->b_linep) {
				ewprintf("Error: Undo stuff called with a"
				    "nonexistent line\n");
				return FALSE;
			}
		}
		count += llength(p) + 1;
	}
	count += off;

	return count;
}

static int
find_linep(int pos, LINE **olp, int *offset)
{
	LINE *p;

	p = curwp->w_linep;
	while (pos > 0 && pos > llength(p)) {
		pos -= llength(p) + 1;
		if ((p = lforw(p)) == curbp->b_linep) {
			*olp = NULL;
			*offset = 0;
			return FALSE;
		}
	}
	*olp = p;
	*offset = pos;

	return TRUE;
}

static struct undo_rec *
new_undo_record(void)
{
	struct undo_rec *rec;

	while (undo_list_num >= MAX_LIST_RECORDS) {
		drop_oldest_undo_record();
		undo_list_num--;
	}
	undo_list_num++;
	rec = LIST_FIRST(&undo_free);
	if (rec != NULL)
		LIST_REMOVE(rec, next);	/* Remove it from the free-list */
	else {
		if ((rec = malloc(sizeof *rec)) == NULL)
			panic("Out of memory in undo code (record)");
	}
	memset(rec, 0, sizeof(struct undo_rec));
	
	return rec;
}

static void
free_undo_record(struct undo_rec *rec)
{
	if (rec->content != NULL) {
		free(rec->content);
		rec->content = NULL;
	}
	if (undo_free_num >= MAX_FREE_RECORDS) {
		free(rec);
		return;
	}
	undo_free_num++;
	
	LIST_INSERT_HEAD(&undo_free, rec, next);
}

/*
 * Drop the oldest undo record in our list. Return 1 if we could remove it,
 * 0 if the undo list was empty
 */
static int
drop_oldest_undo_record(void)
{
	struct undo_rec *rec;

	rec = LIST_END(&undo_list);
	if (rec != NULL) {
		undo_free_num--;
		LIST_REMOVE(rec, next);	/* Remove it from the undo_list before
					 * we insert it in the free list */
		free_undo_record(rec);
		return 1;
	}
	return 0;
}
	
int
undo_init(void)
{
	LIST_INIT(&undo_free);
	LIST_INIT(&undo_list);
	
	return TRUE;
}

int
undo_enable(int on)
{
	undo_disable_flag = on ? 0 : 1;
	
	/*
	 * XXX-Vince:
	 *
	 * Here, I wonder if we should assume that the user has made a
	 * long term choice.  If so, we could free all internal undo
	 * data and save memory.
	 */
	
	return on;
}

int
undo_add_boundary(void)
{
	struct undo_rec *rec;

	if (undo_disable_flag)
		return TRUE;

	rec = new_undo_record();
	rec->buf = curbp;
	rec->type = BOUNDARY;

	LIST_INSERT_HEAD(&undo_list, rec, next);
	
	return TRUE;
}

int
undo_add_insert(LINE *lp, int offset, int size)
{
	REGION reg;
	struct undo_rec *rec;
	
	if (undo_disable_flag)
		return TRUE;

	reg.r_linep = lp;
	reg.r_offset = offset;
	reg.r_size = size;
	
	/*
	 * We try to reuse the last undo record to `compress' things.
	 */	
	rec = LIST_FIRST(&undo_list);
	if ((rec != NULL) &&
	    (rec->type == INSERT) &&
	    (rec->buf == curbp) &&
	    (rec->region.r_linep == lp)) {
		int dist;

		dist = rec->region.r_offset - reg.r_offset;
		
		if (rec->region.r_size >= dist) {
			rec->region.r_size += reg.r_size;
			return TRUE;
		}
	}
	/*
	 * We couldn't reuse the last undo record, so prepare a new one
	 */
	rec = new_undo_record();
	rec->pos = find_offset(lp, offset);
	rec->buf = curbp;
	rec->type = INSERT;
	memmove(&rec->region, &reg, sizeof(REGION));
	rec->content = NULL;
	LIST_INSERT_HEAD(&undo_list, rec, next);

	return TRUE;
}

/*
 * This of course must be done _before_ the actual deletion is done
 */
int
undo_add_delete(LINE *lp, int offset, int size)
{
	REGION reg;
	struct undo_rec *rec;
	int dist, pos;

	if (undo_disable_flag)
		return TRUE;

	reg.r_linep = lp;
	reg.r_offset = offset;
	reg.r_size = size;

	pos = find_offset(lp, offset);

	/*
	 * Again, try to reuse last undo record, if we can
	 */
	rec = LIST_FIRST(&undo_list);
	if ((rec != NULL) &&
	    (rec->type == DELETE) &&
	    (rec->buf == curbp) &&
	    (rec->region.r_linep == reg.r_linep)) {
		char *newbuf;
		int newlen;

		dist = rec->region.r_offset - reg.r_offset;
		if (rec->region.r_size >= dist) {
			newlen = rec->region.r_size + reg.r_size;

			do {
				newbuf = malloc(newlen * sizeof(char));
			} while (newbuf == NULL && drop_oldest_undo_record());
			
			if (newbuf == NULL)
				panic("out of memory in undo delete code");

			/*
			 * [new data][old data]
			 */
			region_get_data(&reg, newbuf, size);
			memmove(newbuf + reg.r_size, rec->content,
			    rec->region.r_size);

			rec->pos = pos;
			rec->region.r_offset = reg.r_offset;
			rec->region.r_size = newlen;
			if (rec->content != NULL)
				free(rec->content);
			rec->content = newbuf;

			return TRUE;
		}
	}

	/*
	 * So we couldn't reuse the last undo record? Just allocate a new
	 * one.
	 */
	rec = new_undo_record();
	rec->pos = pos;

	rec->buf = curbp;
	rec->type = DELETE;
	memmove(&rec->region, &reg, sizeof(REGION));
	do {
		rec->content = malloc(reg.r_size + 1);
	} while ((rec->content == NULL) && drop_oldest_undo_record());
	
	if (rec->content == NULL)
		panic("Out of memory");
	
	region_get_data(&reg, rec->content, reg.r_size);

	LIST_INSERT_HEAD(&undo_list, rec, next);

	return TRUE;
}

/*
 * This of course must be called before the change takes place
 */
int
undo_add_change(LINE *lp, int offset, int size)
{
	REGION reg;
	struct undo_rec *rec;

		
	if (undo_disable_flag)
		return TRUE;
	
	reg.r_linep = lp;
	reg.r_offset = offset;
	reg.r_size = size;
	
	rec = new_undo_record();
	rec->pos = find_offset(lp, offset);
	rec->buf = curbp;
	rec->type = CHANGE;
	memmove(&rec->region, &reg, sizeof reg);

	do {
		rec->content = malloc(size + 1);
	} while ((rec->content == NULL) && drop_oldest_undo_record());
	
	if (rec->content == NULL)
		panic("Out of memory in undo change code");

	region_get_data(&reg, rec->content, size);

	LIST_INSERT_HEAD(&undo_list, rec, next);
	
	return TRUE;
}

int
undo(void)
{
	struct undo_rec *rec;
	LINE *ln;
	int off;

 again:
	rec = LIST_FIRST(&undo_list);
	if (rec == NULL) {
		ewprintf("Nothing to undo!");
		return FALSE;
	}
	if (rec->buf != curbp)
		popbuf(rec->buf);

	LIST_REMOVE(rec, next);
	if (rec->type == BOUNDARY)
		goto again;

	/*
	 * Let called functions know they are below us (for example, ldelete
	 * don't want to record an undo record when called by us)
	 */
	undoaction++;

	find_linep(rec->pos, &ln, &off);
	if (ln == NULL)
		return FALSE;
	
	/*
	 * Move to where this record has to apply
	 */
	curwp->w_dotp = ln;
	curwp->w_doto = off;

	switch (rec->type) {
	case INSERT:
		ldelete(rec->region.r_size, KFORW);
		break;
	case DELETE:
		region_put_data(rec->content, rec->region.r_size);
		break;
	case CHANGE:
		forwchar(0, rec->region.r_size);
		lreplace(rec->region.r_size, rec->content, 1);
		break;
	default:
		break;
	}

	free_undo_record(rec);

	undoaction--;
	
	return TRUE;
}
