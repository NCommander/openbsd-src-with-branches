/*
 * Copyright (c) 1998, 1999, 2000 Kungliga Tekniska H�gskolan
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
#include <rx/rx.h>
#include <rx/rx_null.h>

#include <ports.h>
#include <ko.h>
#include <netinit.h>
#include <msecurity.h>

#ifdef KERBEROS
#include <des.h>
#include <krb.h>
#include <rxkad.h>
#endif

#include <err.h>
#include <assert.h>
#include <ctype.h>
#include <getarg.h>

#ifndef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <service.h>

#include "pts.h"
#include "pts.ss.h"
#include "ptserver.h"
#include "pts.ss.h"

RCSID("$Id: ptserver.c,v 1.24 2000/08/21 00:09:53 tol Exp $");

static struct rx_service *prservice;

static int pr_database = -1;
static off_t file_length;
prheader pr_header;

char pr_header_ydr[PRHEADER_SIZE];


static int ptdebug = 0;

int
pt_setdebug (int debug)
{
    int odebug = ptdebug;
    ptdebug = debug;
    return odebug;
}

void
pt_debug (char *fmt, ...)
{
    va_list args;
    if (!ptdebug)
	return ;

    va_start (args, fmt);
    vfprintf (stderr, fmt, args);
    va_end(args);
}


static void
write_header(void)
{
    off_t pos;
    int length = PRHEADER_SIZE;

    if (ydr_encode_prheader(&pr_header, pr_header_ydr, &length) == NULL)
	err(1, "write_header");

    pos = lseek(pr_database, 0, SEEK_SET);
    assert(pos == 0);

    length = write(pr_database, pr_header_ydr, PRHEADER_SIZE);
    assert (length == PRHEADER_SIZE);
}

static void
read_header(void)
{
    char pr_header_ydr[PRHEADER_SIZE];
    int length = PRHEADER_SIZE;

    if (lseek(pr_database, 0, SEEK_SET) == -1)
	err(1, "lseek");

    length = read(pr_database, pr_header_ydr, PRHEADER_SIZE);
    if (length == -1)
	err(1, "read");
    if (length != PRHEADER_SIZE)
	errx(1, "read_header read failed");
    
    if (ydr_decode_prheader(&pr_header, pr_header_ydr, &length) == NULL)
	err(1, "read_header");
}

static void
get_file_length(void)
{
    file_length = lseek(pr_database, 0, SEEK_END);
    if (file_length == -1) {
	err(1, "lseek");
    }
}


char *
localize_name(const char *name)
{
    static prname localname;
    char *tmp;

    strlcpy(localname, name, sizeof(localname));
    strlwr(localname);
    
    if (tmp = strchr(localname, '@'))
	if (!strcasecmp(tmp + 1, netinit_getrealm()))
	    *tmp = '\0';

    return localname;
}

static void
create_database(void)
{
    pr_header.version = 0;
    pr_header.headerSize = PRHEADER_SIZE;
    pr_header.freePtr = 0;
    pr_header.eofPtr = PRHEADER_SIZE;
    pr_header.maxGroup = -210; /* XXX */
    pr_header.maxID = 0;
    pr_header.maxForeign = 65536; /* XXX */
    pr_header.maxInst = 0;
    pr_header.orphan = 0;
    pr_header.usercount = 0;
    pr_header.groupcount = 0;
    pr_header.foreigncount = 0;
    pr_header.instcount = 0;
    memset(pr_header.reserved, 0, 5 * sizeof(int32_t));
    memset(pr_header.nameHash, 0, 8191 * sizeof(int32_t));
    memset(pr_header.idHash, 0, 8191 * sizeof(int32_t));
    write_header();
    get_file_length();
}

static off_t
find_first_free(void)
{
    off_t pos;

    if (pr_header.freePtr == 0) { /* if there are no free entries */
	pos = lseek(pr_database, 0, SEEK_END);
	if (pos == -1)
	    err(1, "lseek");
	if (ftruncate(pr_database, pos + PRENTRY_DISK_SIZE) == -1)
	    err(1, "ftruncate");
	return pos;
    } else { /* there are free entries */
	/* Not implemented yet */
	assert(0);
    }
    return 0;
}

static int
write_entry(off_t offset, prentry *pr_entry)
{
    off_t pos;
    char pr_entry_disk_ydr[PRENTRY_DISK_SIZE];
    int length = PRENTRY_DISK_SIZE;

    if (ydr_encode_prentry_disk((prentry_disk *) pr_entry, pr_entry_disk_ydr, &length) == NULL)
	err(1, "write_entry");

    pos = lseek(pr_database, offset, SEEK_SET);
    assert(pos == offset);

    length = write(pr_database, pr_entry_disk_ydr, PRENTRY_DISK_SIZE);
    assert (length == PRENTRY_DISK_SIZE);

    return 0;
}

static int
read_entry(off_t offset, prentry *pr_entry)
{
    off_t pos;
    char pr_entry_disk_ydr[PRENTRY_DISK_SIZE];
    int length = PRENTRY_DISK_SIZE;

    pos = lseek(pr_database, offset, SEEK_SET);
    assert(pos == offset);

    length = read(pr_database, pr_entry_disk_ydr, PRENTRY_DISK_SIZE);
    assert (length == PRENTRY_DISK_SIZE);

    if (ydr_decode_prentry_disk((prentry_disk *) pr_entry, pr_entry_disk_ydr, &length) == NULL)
	err(1, "write_entry");

    return 0;
}

static unsigned long
get_id_hash(long id)
{
    return ((unsigned long) id) % HASHSIZE;
}

static unsigned long
get_name_hash(const char *name)
{
    int i;
    unsigned long hash = 0x47114711;

    for (i = 0; name[i] && i < 32; i++)
	hash *= name[i];

    return hash % HASHSIZE;
}

static int
get_first_id_entry(unsigned long hash_id, prentry *pr_entry)
{
    off_t offset = pr_header.idHash[hash_id];
    int status;

    pt_debug ("get_first_id_entry hash_id: %lu offset: %ld\n",
	    hash_id, (long)offset);
    if (offset == 0)
	return PRNOENT;

    status = read_entry(offset, pr_entry);

    return status;
}

static int
get_first_name_entry(unsigned long hash_name, prentry *pr_entry)
{
    off_t offset = pr_header.nameHash[hash_name];
    int status;

    pt_debug ("get_first_name_entry hash_name: %lu offset: %ld\n",
	    hash_name, (long)offset);
    if (offset == 0)
	return PRNOENT;

    status = read_entry(offset, pr_entry);

    return status;
}

static int
update_entry(prentry *pr_entry)
{
    off_t offset;
    int status;
    unsigned long hash_id;
    prentry old_pr_entry;

    fprintf(stderr, "update_entry\n");

    hash_id = get_id_hash(pr_entry->id);

    offset = pr_header.idHash[hash_id];

    status = get_first_id_entry(hash_id, &old_pr_entry);
    if (status)
	return PRNOENT;

    while (old_pr_entry.id != pr_entry->id) {
	if (old_pr_entry.nextID == 0)
	    return PRNOENT;
	offset=old_pr_entry.nextID;
	status = read_entry(offset, &old_pr_entry);
    }

    return write_entry(offset, pr_entry);
}

static int
insert_entry(prentry *pr_entry)
{
    off_t offset;
    int status;
    unsigned long hash_id, hash_name;
    prentry first_id_entry;
    prentry first_name_entry;
    
    /* Allokera plats i filen */
    offset = find_first_free();

    /* Hitta plats i hashtabell */
    hash_id = get_id_hash(pr_entry->id);
    hash_name = get_name_hash(pr_entry->name);

    status = get_first_id_entry(hash_id, &first_id_entry);
    pr_entry->nextID = status ? 0 : first_id_entry.nextID;

    status = get_first_name_entry(hash_name, &first_name_entry);
    pr_entry->nextName = status ? 0 : first_name_entry.nextName;

    /* XXX: uppdatera owned och nextOwned */

    /* L�gg in entryt i filen */
    status = write_entry(offset, pr_entry);
    if (status)
	return status;
    
    /* Uppdatera hashtabell */
    pr_header.idHash[hash_id] = offset;
    pr_header.nameHash[hash_name] = offset;
    write_header();
    return 0;
}

int
create_group(const char *name,
	     int32_t id,
	     int32_t owner,
	     int32_t creator)
{
    int status;
    prentry pr_entry;
    
    memset(&pr_entry, 0, sizeof(pr_entry));
    pr_entry.flags = PRGRP;
    pr_entry.id = id;
    pr_entry.owner = owner;
    pr_entry.creator = creator;
    strlcpy(pr_entry.name, name, PR_MAXNAMELEN);

    status = insert_entry(&pr_entry);
    if (status)
	return status;

    return 0;
}

int
create_user(const char *name,
	    int32_t id,
	    int32_t owner,
	    int32_t creator)
{
    int status;
    prentry pr_entry;
    
    memset(&pr_entry, 0, sizeof(pr_entry));
    pr_entry.flags = 0;
    pr_entry.id = id;
    pr_entry.owner = owner;
    pr_entry.creator = creator;
    strlcpy(pr_entry.name, name, PR_MAXNAMELEN);

    status = insert_entry(&pr_entry);
    if (status)
	return status;

    return 0;
}

int
addtogroup (int32_t uid, int32_t gid) {
    prentry uid_entry;
    prentry gid_entry;
    int error,i;

    fprintf(stderr, "addtogroup\n");

    if (error = get_pr_entry_by_id(uid, &uid_entry))
	return error;

    if (error = get_pr_entry_by_id(gid, &gid_entry))
	return error;

    /* XXX should allocate contentry block */
    
    if (uid_entry.count >= PRSIZE || gid_entry.count >= PRSIZE)
	return PRNOENT;

    assert (uid_entry.entries[uid_entry.count] == 0);
    uid_entry.entries[uid_entry.count] = gid;
    uid_entry.count++;

    assert (gid_entry.entries[gid_entry.count] == 0);
    gid_entry.entries[gid_entry.count] = uid;
    gid_entry.count++;

    if ((error = update_entry(&uid_entry)) != 0)
	return error;

    if ((error = update_entry(&gid_entry)) != 0)
	return error;

    return 0;

}
int
removefromgroup (int32_t uid, int32_t gid) {
    prentry uid_entry;
    prentry gid_entry;
    int error, i, offset;

    fprintf(stderr, "removefromgroup\n");

    if (error = get_pr_entry_by_id(uid, &uid_entry))
	return error;

    if (error = get_pr_entry_by_id(gid, &gid_entry))
	return error;

    /* XXX No check for full list */

    /* XXX should the list be sorted? */

    error = PRNOENT;  /* XXX */
    for (i = 0; i < PRSIZE; i++)
	if (uid_entry.entries[i] == gid) {
	    uid_entry.count--;
	    uid_entry.entries[i] = uid_entry.entries[uid_entry.count];
	    uid_entry.entries[uid_entry.count] = 0;
	    error = 0;
	}
    if (error)
	return error;


    error = PRNOENT;  /* XXX */
    for (i = 0; i < PRSIZE; i++)
	if (gid_entry.entries[i] == uid) {
	    gid_entry.count--;
	    gid_entry.entries[i] = gid_entry.entries[gid_entry.count];
	    gid_entry.entries[gid_entry.count] = 0;
	    error = 0;
	}
    if (error)
	return error;


    /* XXX may leave database inconsistent ?? */

    if ((error = update_entry(&uid_entry)) != 0)
	return error;
    
    if ((error = update_entry(&gid_entry)) != 0)
	return error;

    return 0;

}

int
listelements (int32_t id, prlist *elist, Bool default_id_p)
{
    prentry pr_entry;
    int i = 0, error;

    if (error = get_pr_entry_by_id (id, &pr_entry))
        return error;
    
    elist->val = malloc(sizeof(*elist->val)
			* (pr_entry.count + (default_id_p ? 3 : 1)));
    if (elist->val == NULL)
	return ENOMEM; /* XXX */

    
    /* XXX contentry blocks... */
    /* XXX should be sorted */

    for (i = 0; i < pr_entry.count; i++)
	    elist->val[i] = pr_entry.entries[i];

    elist->val[i] = id;
    if (default_id_p) {
	elist->val[++i] = PR_ANYUSERID;
	elist->val[++i] = PR_AUTHUSERID;
	elist->len = pr_entry.count + 3;
    } else
	elist->len = pr_entry.count + 1;

    return 0;
}


int
get_pr_entry_by_id(int id, prentry *pr_entry)
{
    unsigned long hash_id = get_id_hash(id);
    int status;

    pt_debug ("get_pr_entry_by_id id:%d hash_id: %ld\n", id, hash_id);

    status = get_first_id_entry(hash_id, pr_entry);
    pt_debug ("get_pr_entry_by_id status:%d\n", status);
    if (status)
	return PRNOENT;

    while (pr_entry->id != id) {
	if (pr_entry->nextID == 0)
	    return PRNOENT;
	status = read_entry(pr_entry->nextID, pr_entry);
    }

    pt_debug ("pr_entry->id: %d\n", pr_entry->id);
    pt_debug ("pr_entry->owner: %d\n", pr_entry->owner);
    pt_debug ("pr_entry->creator: %d\n", pr_entry->creator);
    pt_debug ("pr_entry->name: %s\n", pr_entry->name);

    return 0;
}

int
get_pr_entry_by_name(const char *name, prentry *pr_entry)
{
    int hash_name = get_name_hash(name);
    int status;

    status = get_first_name_entry(hash_name, pr_entry);
    if (status)
	return PRNOENT;

    while (strcmp(pr_entry->name, name)) {
	if (pr_entry->nextName == 0)
	    return PRNOENT;
	status = read_entry(pr_entry->nextName, pr_entry);
    }

    fprintf(stderr, "  pr_entry->id: %d\n", pr_entry->id);
    fprintf(stderr, "  pr_entry->owner: %d\n", pr_entry->owner);
    fprintf(stderr, "  pr_entry->creator: %d\n", pr_entry->creator);
    fprintf(stderr, "  pr_entry->name: %s\n", pr_entry->name);

    return 0;
}

int
conv_name_to_id(const char *name, int *id)
{
    prentry pr_entry;
    int status;

    status = get_pr_entry_by_name(name, &pr_entry);
    if (status)
	return status;
    *id = pr_entry.id;
    return 0;
}

int
conv_id_to_name(int id, char *name)
{
    prentry pr_entry;
    int status;

    status = get_pr_entry_by_id(id, &pr_entry);
    if (status)
	return status;
    strlcpy(name, pr_entry.name, PR_MAXNAMELEN);
    return 0;
}

int
next_free_group_id(void)
{
    pr_header.maxGroup--; /* XXX */
    write_header();
    return pr_header.maxGroup;
}

int
next_free_user_id()
{
    pr_header.maxID++; /* XXX */
    write_header();
    return pr_header.maxID;
}


/*
 * Open the pr database that lives in ``databaseprefix''
 * with open(2) ``flags''. Returns 0 or errno.
 */

static int
open_db(char *databaseprefix, int flags)
{
    char database[MAXPATHLEN];

    assert (pr_database == -1);

    if (databaseprefix == NULL)
	databaseprefix = MILKO_SYSCONFDIR;

    snprintf (database, sizeof(database), "%s/pr_database", 
	      databaseprefix);

    pr_database = open(database, flags, S_IRWXU);
    if (pr_database < 0)
	return errno;
    return 0;
}

static int
prserver_create(char *databaseprefix)
{
    int status;

    printf ("Creating a new pr-database.\n");

    status = open_db(databaseprefix, O_RDWR|O_CREAT|O_BINARY|O_EXCL);
    if (status)
	errx (1, "failed open_db with error: %s (%d)",
	      strerror(status), status);

    create_database();

#define M(N,I,O,G) \
    do { \
	status = create_group((N), (I), (O), (G)); \
	if (status)  \
            errx (1, "failed when creating %s with error %d", \
	          (N), status); \
    } while (0)

    M("system:administrators", PR_SYSADMINID, PR_SYSADMINID, PR_SYSADMINID);
    M("system:anyuser", PR_ANYUSERID, PR_SYSADMINID, PR_SYSADMINID);
    M("system:authuser", PR_AUTHUSERID, PR_SYSADMINID, PR_SYSADMINID);
    M("anonymous", PR_ANONYMOUSID, PR_SYSADMINID, PR_SYSADMINID);

#undef M

    return 0;
}

static int
prserver_init(char *databaseprefix)
{
    int status;
    
    status = open_db(databaseprefix, O_RDWR|O_BINARY);
    if (status)
	errx (1, "failed open_db with error: %s (%d)",
	      strerror(status), status);

    read_header();
    get_file_length();

    return 0;
}

static char *cell = NULL;
static char *realm = NULL;
static char *databasedir = NULL;
static char *srvtab_file = NULL;
static int no_auth = 0;
static int do_help = 0;
static int do_debug = 0;
static int do_create = 0;

static struct getargs args[] = {
    {"create",  0, arg_flag,      &do_create, "create new databas"},
    {"cell",	0, arg_string,    &cell, "what cell to use"},
    {"realm",	0, arg_string,	  &realm, "what realm to use"},
    {"prefix",'p', arg_string,    &databasedir, "what dir to store the db"},
    {"noauth", 0,  arg_flag,	  &no_auth, "disable authentication checks"},
    {"srvtab",'s', arg_string,    &srvtab_file, "what srvtab to use"},
    {"debug",  'd', arg_flag,     &do_debug, "enable debug messages"},
    {"help",  'h', arg_flag,      &do_help, "help"},
    { NULL, 0, arg_end, NULL }
};

static void
usage(int exit_code)
{
    arg_printusage(args, NULL, "", ARG_AFSSTYLE);
    exit (exit_code);
}

/*
 *
 */

int
main(int argc, char **argv) 
{
    int ret;
    int optind = 0;

    if (getarg (args, argc, argv, &optind, ARG_AFSSTYLE)) {
	usage (1);
    }

    argc -= optind;
    argv += optind;

    if (argc) {
	printf("unknown option %s\n", *argv);
	return 1;
    }

    if (do_help)
	usage(0);

    if (do_debug)
	pt_setdebug (1);

    if (do_create) {
	prserver_create (databasedir);
	return 0;
    }

    if (no_auth)
	sec_disable_superuser_check ();

    printf ("ptserver booting\n");

    ports_init();
    cell_init(0);

    if (cell)
	cell_setthiscell (cell);

    network_kerberos_init (srvtab_file);
    
    ret = prserver_init(databasedir);
    if (ret)
	errx (1, "prserver_init: error %d\n", ret);

    ret = network_init(htons(afsprport), "pr", PR_SERVICE_ID, 
		       PR_ExecuteRequest, &prservice, NULL);
    if (ret)
	errx (1, "network_init returned %d", ret);

    printf("started\n");

    rx_SetMaxProcs(prservice,5) ;
    rx_StartServer(1) ;

    abort();
    return 0;
}
