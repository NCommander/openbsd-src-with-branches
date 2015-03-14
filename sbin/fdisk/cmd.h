/*	$OpenBSD: cmd.h,v 1.13 2014/03/31 22:03:29 krw Exp $	*/

/*
 * Copyright (c) 1997 Tobias Weingartner
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

#ifndef _CMD_H
#define _CMD_H

#define CMD_EXIT	0x0000
#define CMD_SAVE	0x0001
#define CMD_CONT	0x0002
#define CMD_CLEAN	0x0003
#define CMD_DIRTY	0x0004

struct cmd {
	char *cmd;
	int (*fcn)(char *, struct disk *, struct mbr *, struct mbr *,
	    int);
	char *help;
};
extern struct cmd cmd_table[];

int Xreinit(char *, struct  disk *, struct mbr *, struct mbr *, int);
int Xdisk(char *, struct disk *, struct mbr *, struct mbr *, int);
int Xmanual(char *, struct disk *, struct mbr *, struct mbr *, int);
int Xedit(char *, struct disk *, struct mbr *, struct mbr *, int);
int Xsetpid(char *, struct disk *, struct mbr *, struct mbr *, int);
int Xselect(char *, struct disk *, struct mbr *, struct mbr *, int);
int Xswap(char *, struct disk *, struct mbr *, struct mbr *, int);
int Xprint(char *, struct disk *, struct mbr *, struct mbr *, int);
int Xwrite(char *, struct disk *, struct mbr *, struct mbr *, int);
int Xexit(char *, struct disk *, struct mbr *, struct mbr *, int);
int Xquit(char *, struct disk *, struct mbr *, struct mbr *, int);
int Xabort(char *, struct disk *, struct mbr *, struct mbr *, int);
int Xhelp(char *, struct disk *, struct mbr *, struct mbr *, int);
int Xflag(char *, struct disk *, struct mbr *, struct mbr *, int);
int Xupdate(char *, struct disk *, struct mbr *, struct mbr *, int);

#endif /* _CMD_H */
