/* ==== specific.h ========================================================
 * Copyright (c) 1994 by Chris Provenzano, proven@mit.edu
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
 * $Id: specific.h,v 1.52 1994/09/29 06:19:25 proven Exp $
 *
 * Description : Thread specific data management header.
 *
 *  1.20 94/03/30 proven
 *      -Started coding this file.
 */

#define	PTHREAD_DATAKEYS_MAX		256
#define _POSIX_THREAD_DESTRUTOR_ITERATIONS	4

/*
 * New thread specific key type.
 */
struct pthread_key {
	pthread_mutex_t			mutex;
	long					count;
	void 					(*destructor)();
};

typedef int pthread_key_t;

/*
 * New functions
 */

__BEGIN_DECLS

int	pthread_key_create			__P_((pthread_key_t *, void (*routine)(void *)));
int	pthread_setspecific			__P_((pthread_key_t, const void *));
void *pthread_getspecific		__P_((pthread_key_t));
int	pthread_key_delete			__P_((pthread_key_t));
		
__END_DECLS

