/* ==== kernel.h ============================================================
 * Copyright (c) 1993 by Chris Provenzano, proven@mit.edu
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
 * $Id: kernel.h,v 1.52 1994/12/13 07:09:01 proven Exp $
 *
 * Description : mutex header.
 *
 *  1.00 93/07/22 proven
 *      -Started coding this file.
 */
 
/*
 * Defines only for the pthread user kernel.
 */
#if defined(PTHREAD_KERNEL)

#ifdef __GNUC__
#include <assert.h>
#endif
#ifdef __ASSERT_FUNCTION
#define PANIC() panic_kernel( __FILE__, __LINE__, __ASSERT_FUNCTION )
#else
#define PANIC() panic_kernel( __FILE__, __LINE__, (const char *)0 )
#endif


/* Time each rr thread gets */
#define PTHREAD_RR_TIMEOUT			100000000

/* Set the errno value */
#define SET_ERRNO(x)								\
{													\
	if (!pthread_run->error_p) {					\
		pthread_run->error_p = &pthread_run->error;	\
	}												\
	(*(pthread_run->error_p)) = x;					\
}

/* Globals only the internals should see */
extern struct pthread_prio_queue  *	pthread_current_prio_queue;
extern volatile int					pthread_kernel_lock;

#endif
