/* ==== globals.c ============================================================
 * Copyright (c) 1993, 1994 by Chris Provenzano, proven@mit.edu
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
 *  This product includes software developed by Chris Provenzano.
 * 4. The name of Chris Provenzano may not be used to endorse or promote 
 *	  products derived from this software without specific prior written
 *	  permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CHRIS PROVENZANO ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL CHRIS PROVENZANO BE LIABLE FOR ANY 
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 *
 * Description : Global variables.
 *
 *  1.00 93/07/26 proven
 *      -Started coding this file.
 */

#ifndef lint
static const char rcsid[] = "$Id: globals.c,v 1.3 1994/02/07 22:04:19 proven Exp $ $provenid: globals.c,v 1.16 1994/02/07 02:18:57 proven Exp $";
#endif

#include <pthread.h>

/*
 * Initial thread, running thread, and top of link list
 * of all threads.
 */
struct pthread *pthread_run;
struct pthread *pthread_initial;
struct pthread *pthread_link_list;

/*
 * default thread attributes
 */
pthread_attr_t pthread_default_attr = { SCHED_RR, NULL, PTHREAD_STACK_DEFAULT };

/*
 * Queue for all threads elidgeable to run this scheduling round.
 */
struct pthread_queue pthread_current_queue = PTHREAD_QUEUE_INITIALIZER;

/*
 * File table information
 */
struct fd_table_entry *fd_table[64];


