/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      H�gskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 *  Include file for whole arlad
 *  $Id: arla_local.h,v 1.55 2000/07/02 16:24:37 assar Exp $
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <time.h>
#include <limits.h>
#include <errno.h>
#include <sys/time.h>
#if defined(HAVE_DIRENT_H)
#define _KERNEL
#include <dirent.h>
#undef _KERNEL
#if DIRENT_AND_SYS_DIR_H
#include <sys/dir.h>
#endif
#elif defined(HAVE_SYS_DIR_H)
#include <sys/dir.h>
#endif
#include <unistd.h>
#include <sys/types.h>
#ifdef HAVE_LINUX_TYPES_H
#include <linux/types.h>
#endif
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/uio.h>
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#include <sys/stat.h>
#ifdef HAVE_SYS_IOCCOM_H
#include <sys/ioccom.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <pwd.h>
#include <err.h>
#include <parse_units.h>
#include <roken.h>

#include <lwp.h>
#include <lock.h>

#include <rx/rx.h>
#include <rx/rx_null.h>
#include <rx/rxgencon.h>

#ifdef KERBEROS
#include <des.h>
#include <krb.h>
#include <rxkad.h>
#endif

#ifdef USE_MMAPTIME
#include <mmaptime.h>
#endif

#include <kafs.h>

#include "log.h"

#include "fs.h"
#include "cmcb.h"
#include "fs.cs.h"
#include "list.h"
#include "vldb.h"
#include "vldb.cs.h"
#include "volcache.h"
#include "fbuf.h"
#include "hash.h"
#include "afs_dir.h"
#include "service.h"
#include "ports.h"
#include "conn.h"
#include "fcache.h"
#include "inter.h"
#include "cred.h"
#include "adir.h"
#include "service.h"
#include "subr.h"
#include "fprio.h"
#include "bool.h"
#include "kernel.h"
#include "messages.h"
#include "fs_errors.h"
#include "arladeb.h"
#include "ko.h"
#include "xfs.h"

enum connected_mode { CONNECTED  = 0,
		      FETCH_ONLY = 1,
		      DISCONNECTED = 2,
                      CONNECTEDLOG = 4};

extern enum connected_mode connected_mode;

#include "darla.h"
#include "discon_log.h"
#include "discon.h"
#include "reconnect.h"

#include "dynroot.h"

#define SYSNAMEMAXLEN 2048
extern char arlasysname[SYSNAMEMAXLEN];


#define ARLA_NUMCONNS 200
#define ARLA_HIGH_VNODES 4000
#define ARLA_LOW_VNODES 3000
#define ARLA_HIGH_BYTES 40000000
#define ARLA_LOW_BYTES 30000000
#define ARLA_NUMCREDS 200
#define ARLA_NUMVOLS 100

/* 
 * This should be a not used uid in the system, 
 * XFS_ANONYMOUSID may be good
 */
#define ARLA_NO_AUTH_CRED 4

extern int fake_mp;
