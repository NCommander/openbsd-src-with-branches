/*
 * Copyright (c) 1998, 1999 Kungliga Tekniska H�gskolan
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <ctype.h>
#include <assert.h>

#include <rx/rx.h>
#include <rx/rx_null.h>

#include <ports.h>
#include <bool.h>

#ifdef KERBEROS
#include <des.h>
#include <krb.h>
#include <rxkad.h>
#include "rxkad_locl.h"
#endif

#include <err.h>

#ifndef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <service.h>

#include "pts.h"
#include "pts.ss.h"
#include "ptserver.h"
#include "pts.ss.h"

#include "msecurity.h"

RCSID("$Id: pr.c,v 1.17 2000/08/16 23:08:19 tol Exp $");

int PR_NameToID(struct rx_call *call,
		const namelist *nlist,
		idlist *ilist)
{
    int i;
    int status;
    char *localname;

    printf("PR_NameToID\n");
/*    printf("  securityIndex: %d\n", call->conn->securityIndex);*/
#ifdef KERBEROS
    if (call->conn->securityIndex == 2) {
	serv_con_data *cdat = call->conn->securityData;
	printf("  user: %s.%s@%s\n",
	       cdat->user->name,
	       cdat->user->instance,
	       cdat->user->realm);
    }
#endif

    ilist->len = nlist->len;
    ilist->val = malloc(sizeof(int) * ilist->len);
    if (ilist->val == NULL)
	return PRDBBAD;

    for (i = 0; i < nlist->len; i++) {
	printf("  name: %s\n", nlist->val[i]);
	
	localname = localize_name(nlist->val[i]);

	status = conv_name_to_id(localname, &ilist->val[i]);
	if (status == PRNOENT)
	    ilist->val[i] = PR_ANONYMOUSID;
	else if (status)
	    return status;
    }
    return 0;
}

int
PR_IDToName(struct rx_call *call,
	    const idlist *ilist,
	    namelist *nlist)
{
    int i;
    int status;

    printf("PR_IDToName\n");

    
    if (ilist->len < 0 || ilist->len >= PR_MAXLIST)
	return PRTOOMANY;

    nlist->len = ilist->len;

    if (ilist->len == 0) {
	nlist->val = NULL;
	return 0;
    }

    nlist->val = calloc(1, sizeof(prname) * nlist->len);
    if (nlist->val == NULL)
	return PRDBBAD;

    for (i = 0; i < ilist->len; i++) {
/*	printf("  id: %d\n", ilist->val[i]);*/
	status = conv_id_to_name(ilist->val[i], nlist->val[i]);
	if (status == PRNOENT)
	    snprintf (nlist->val[i], PR_MAXNAMELEN, "%d", ilist->val[i]);
	else if (status)
	    return status;
    }
    return 0;
}

int PR_NewEntry(struct rx_call *call
    , const char name[ 64 ]
    , const int32_t flag
    , const int32_t oid
    , int32_t *id
    )
{
    int error;
    char *localname;

    printf("PR_NewEntry\n");
    printf("  securityIndex: %d\n", call->conn->securityIndex);
    printf("  name:%s oid:%d\n", name, oid);


/* XXX should be authuser? */
    if (!sec_is_superuser(call))
	return PRPERM;

    localname = localize_name(name);
    if ((flag & PRTYPE) == PRUSER) {
	error = conv_name_to_id(localname, id);
	if (error == PRNOENT) {
	    *id = next_free_user_id();
	    error = create_user(localname, *id, oid, PR_SYSADMINID); /* XXX */
	} else
	    error = PREXIST;
    } else if ((flag & PRTYPE) == PRGRP) {
	error = conv_name_to_id(localname, id);
	if (error == PRNOENT) {
	    *id = next_free_group_id();
	    error = create_group(localname, *id, oid, PR_SYSADMINID); /* XXX */
	} else
	    error = PREXIST;
    } else {
	error = PRPERM;
    }

    return error;
}

int PR_INewEntry(
    struct rx_call *call
    , const char name[ 64 ]
    , const int32_t id
    , const int32_t oid
    )
{
    int error;
    int tempid;
    char *localname;

    if (!sec_is_superuser(call))
	return PRPERM;

    printf("PR_INewEntry\n");
    printf("  securityIndex: %d\n", call->conn->securityIndex);
    printf("  name:%s oid:%d\n", name, oid);

    localname = localize_name(name);
    if (id > 0) {
	error = conv_name_to_id(localname, &tempid);
	if (error == PRNOENT) {
	    error = create_user(localname, id, oid, PR_SYSADMINID); /* XXX */
	} else
	    error = PREXIST;
    } else if (id < 0) {
	error = conv_name_to_id(localname, &tempid);
	if (error == PRNOENT) {
	    error = create_group(localname, id, oid, PR_SYSADMINID); /* XXX */
	} else
	    error = PREXIST;
    } else {
	error = PRPERM;
    }

    return error;
}

int PR_ListEntry(
    struct rx_call *call
    , const int32_t id
    , struct prcheckentry *entry
    )
{
    prentry pr_entry;
    int status;
   
    printf("PR_ListEntry\n");
    printf("  securityIndex: %d\n", call->conn->securityIndex);
    printf("  id:%d\n", id);
#ifdef KERBEROS
    if (call->conn->securityIndex == 2) {
	serv_con_data *cdat = call->conn->securityData;
	printf("  user: %s.%s@%s\n",
	       cdat->user->name,
	       cdat->user->instance,
	       cdat->user->realm);
    }
#endif

    memset(&pr_entry, 0, sizeof(pr_entry));
    status = get_pr_entry_by_id(id, &pr_entry);
    if (status)
	return status;
    entry->flags = pr_entry.flags;
    entry->id = pr_entry.id;
    entry->owner = pr_entry.owner;
    entry->creator = pr_entry.creator;
    entry->ngroups = pr_entry.ngroups;
    entry->nusers = pr_entry.nusers;
    entry->count = pr_entry.count;
    memcpy(entry->reserved, pr_entry.reserved, sizeof(pr_entry.reserved));
    strlcpy(entry->name, pr_entry.name, PR_MAXNAMELEN);

    return 0;
}

int PR_DumpEntry(
    struct rx_call *call
    , const int32_t pos
    , struct prdebugentry *entry
    )
{
    printf("PR_DumpEntry\n");
    return -1;
}

int PR_ChangeEntry(
    struct rx_call *call
    , const int32_t id
    , const char name[ 64 ]
    , const int32_t oid
    , const int32_t newid
    )
{
    printf("PR_ChangeEntry\n");
    return -1;
}


int PR_SetFieldsEntry(
    struct rx_call *call
    , const int32_t id
    , const int32_t mask
    , const int32_t flags
    , const int32_t ngroups
    , const int32_t nusers
    , const int32_t spare1
    , const int32_t spare2
    )
{
    printf("PR_SetFieldsEntry\n");
    return -1;
}


int PR_Delete(
    struct rx_call *call
    , const int32_t id
    )
{
    printf("PR_Delete\n");
    return -1;
}


int PR_WhereIsIt(
    struct rx_call *call
    , const int32_t id
    , int32_t *ps
    )
{
    printf("PR_WhereIsIt\n");
    return -1;
}


int PR_AddToGroup(
    struct rx_call *call
    , const int32_t uid
    , const int32_t gid
    )
{
    printf("PR_AddToGroup\n");

    if (!sec_is_superuser(call))
      return PRPERM;

    return addtogroup(uid,gid);
}


int PR_RemoveFromGroup(
    struct rx_call *call
    , const int32_t id
    , const int32_t gid
    )
{
    printf("PR_RemoveFromGroup\n");

    if (!sec_is_superuser(call))
	return PRPERM;

    return removefromgroup(id, gid);
}


int PR_ListMax(
    struct rx_call *call
    , int32_t *uid
    , int32_t *gid
    )
{
    printf("PR_ListMax\n");
    *uid = pr_header.maxID;
    *gid = pr_header.maxGroup;
    return 0;
}


int PR_SetMax(
    struct rx_call *call
    , const int32_t uid
    , const int32_t gflag
    )
{
    printf("PR_SetMax\n");
    return -1;
}


int PR_ListElements(
    struct rx_call *call
    , const int32_t id
    , prlist *elist
    , int32_t *over
    )
{
    printf("PR_ListElements\n");

    return listelements(id, elist, FALSE);
}


int PR_GetCPS(
    struct rx_call *call
    , const int32_t id
    , prlist *elist
    , int32_t *over
    )
{
    printf("PR_GetCPS\n");

    return listelements(id, elist, TRUE);
}


int PR_ListOwned(
    struct rx_call *call
    , const int32_t id
    , prlist *elist
    , int32_t *over
    )
{
    printf("PR_ListOwned\n");
    return -1;
}


int PR_IsAMemberOf(
    struct rx_call *call
    , const int32_t uid
    , const int32_t gid
    , int32_t *flag
    )
{
    printf("PR_IsAMemberOf\n");
    return -1;
}
