/*	$OpenBSD: kdump_subr.h,v 1.8 2012/12/25 09:35:51 guenther Exp $	*/
/*
 * Copyright(c) 2006 2006 David Kirchner <dpk@dpk.net>
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

/* $FreeBSD: src/usr.bin/kdump/kdump_subr.h,v 1.3 2007/04/09 22:04:27 emaste Exp $ */

void signame(int);
void sigset(int);
void semctlname(int);
void shmctlname(int);
void semgetname(int);
void fcntlcmdname(int, int);
void rtprioname(int);
void modename(int);
void flagsname(int);
void atflagsname(int);
void flagsandmodename(int, int);
void accessmodename(int);
void mmapprotname(int);
void mmapflagsname(int);
void wait4optname(int);
void sendrecvflagsname(int);
void getfsstatflagsname(int);
void mountflagsname(int);
void rebootoptname(int);
void flockname(int);
void sockoptname(int);
void sockoptlevelname(int);
void sockdomainname(int);
void sockipprotoname(int);
void socktypename(int);
void sockfamilyname(int);
void thrcreateflagsname(int);
void mlockallname(int);
void shmatname(int);
void nfssvcname(int);
void whencename(int);
void pathconfname(int);
void rlimitname(int);
void shutdownhowname(int);
void prioname(int);
void madvisebehavname(int);
void msyncflagsname(int);
void clockname(int);
void clocktypename(int);
void schedpolicyname(int);
void kldunloadfflagsname(int);
void ksethrcmdname(int);
void extattrctlname(int);
void kldsymcmdname(int);
void sendfileflagsname(int);
void acltypename(int);
void rusagewho(int);
void sigactionflagname(int);
void sigprocmaskhowname(int);
void lio_listioname(int);
void minheritname(int);
void quotactlname(int);
void ptraceopname(int);

extern int decimal, resolv, fancy;
