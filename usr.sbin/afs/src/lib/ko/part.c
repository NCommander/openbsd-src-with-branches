/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska H�gskolan
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
 * 3. Neither the name of the Institute nor the names of its contributors
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

#include "ko_locl.h"

#include "part.h"

RCSID("$KTH: part.c,v 1.3 2000/10/02 22:43:24 lha Exp $");

/*
 * partition_num2name
 *
 * write the string of the partition number `id' in `str' (which is of
 * size `sz') and return the number of characters written.
 */

int
partition_num2name (int id, char *str, size_t sz)
{
    if (id < 26)
	return snprintf (str, sz, "/vicep%c", 'a' + id);
    else
	return snprintf (str, sz, "/vicep%c%c",
			 'a' + id / 26 - 1, 'a' + id % 26);
}

/*
 * partition_name2num 
 *   convert a char string that might be a partition to a 
 *   number.
 *
 *  returns -1 is there is an error
 *
 */

int
partition_name2num(const char *name)
{
    int ret;

    if (strncmp(name, "/vicep", 6) == 0)
	name += 6;
    else if (strncmp(name, "vicep", 5) == 0)
	name += 5;

    if (*name == '\0')
	return -1;

    if(*(name+1) == '\0') {
	if(isalpha((unsigned char)*name)) {
	    ret = tolower((unsigned char)*name) - 'a';
	} else
	    return -1;
    } else if (name[2] == '\0') {
	if (isalpha((unsigned char)name[0])
	    && isalpha((unsigned char)name[1])) {
	    ret = 26 * (tolower((unsigned char)*(name)) - 'a' + 1)
		+ tolower((unsigned char)*(name+1)) - 'a';
	} else
	    return -1;
    } else
	return -1;

    if(ret > 255)
	return -1;

    return ret;
}
