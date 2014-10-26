/*	$OpenBSD: nfs_prot.h,v 1.5 2014/10/20 02:33:42 guenther Exp $	*/

/*
 * Copyright (c) 1990 Jan-Simon Pendry
 * Copyright (c) 1990 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry at Imperial College, London.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)nfs_prot.h	8.1 (Berkeley) 6/6/93
 */

#include <nfs/nfsproto.h>

#define	xdr_nfsstat xdr_enum
#define	xdr_ftype xdr_enum

#define NFS_PORT 2049
#ifndef NFS_MAXDATA
#define NFS_MAXDATA 8192
#endif
#define NFS_MAXPATHLEN 1024
#define NFS_MAXNAMLEN 255
#define NFS_FHSIZE 32
#define NFS_COOKIESIZE 4
#define NFS_FIFO_DEV -1
#define NFSMODE_FMT 0170000
#define NFSMODE_DIR 0040000
#define NFSMODE_CHR 0020000
#define NFSMODE_BLK 0060000
#define NFSMODE_REG 0100000
#define NFSMODE_LNK 0120000
#define NFSMODE_SOCK 0140000
#define NFSMODE_FIFO 0010000

typedef int nfsstat;

bool_t xdr_nfsstat(XDR *, nfsstat *);


typedef int ftype;

/* static bool_t xdr_ftype(XDR *, ftype *); */


struct nfs_fh {
	char data[NFS_FHSIZE];
};
typedef struct nfs_fh nfs_fh;
bool_t xdr_nfs_fh(XDR *, nfs_fh *);


struct nfstime {
	u_int seconds;
	u_int useconds;
};
typedef struct nfstime nfstime;
/* static bool_t xdr_nfstime(XDR *, nfstime *); */


struct fattr {
	ftype type;
	u_int mode;
	u_int nlink;
	u_int uid;
	u_int gid;
	u_int size;
	u_int blocksize;
	u_int rdev;
	u_int blocks;
	u_int fsid;
	u_int fileid;
	nfstime atime;
	nfstime mtime;
	nfstime ctime;
};
typedef struct fattr fattr;
/* static bool_t xdr_fattr(XDR *, fattr *); */


struct sattr {
	u_int mode;
	u_int uid;
	u_int gid;
	u_int size;
	nfstime atime;
	nfstime mtime;
};
typedef struct sattr sattr;
/* static bool_t xdr_sattr(XDR *, sattr *); */


typedef char *filename;
/* static bool_t xdr_filename(XDR *, filename *); */


typedef char *nfspath;
bool_t xdr_nfspath(XDR *, nfspath *);


struct attrstat {
	nfsstat status;
	union {
		fattr attributes;
	} attrstat_u;
};
typedef struct attrstat attrstat;
bool_t xdr_attrstat(XDR *, attrstat *);


struct sattrargs {
	nfs_fh file;
	sattr attributes;
};
typedef struct sattrargs sattrargs;
bool_t xdr_sattrargs(XDR *, sattrargs *);


struct diropargs {
	nfs_fh dir;
	filename name;
};
typedef struct diropargs diropargs;
bool_t xdr_diropargs(XDR *, diropargs *);


struct diropokres {
	nfs_fh file;
	fattr attributes;
};
typedef struct diropokres diropokres;
bool_t xdr_diropokres(XDR *, diropokres *);


struct diropres {
	nfsstat status;
	union {
		diropokres diropres;
	} diropres_u;
};
typedef struct diropres diropres;
bool_t xdr_diropres(XDR *, diropres *);


struct readlinkres {
	nfsstat status;
	union {
		nfspath data;
	} readlinkres_u;
};
typedef struct readlinkres readlinkres;
bool_t xdr_readlinkres(XDR *, readlinkres *);


struct readargs {
	nfs_fh file;
	u_int offset;
	u_int count;
	u_int totalcount;
};
typedef struct readargs readargs;
bool_t xdr_readargs(XDR *, readargs *);


struct readokres {
	fattr attributes;
	struct {
		u_int data_len;
		char *data_val;
	} data;
};
typedef struct readokres readokres;
bool_t xdr_readokres(XDR *, readokres *);


struct readres {
	nfsstat status;
	union {
		readokres reply;
	} readres_u;
};
typedef struct readres readres;
bool_t xdr_readres(XDR *, readres *);


struct writeargs {
	nfs_fh file;
	u_int beginoffset;
	u_int offset;
	u_int totalcount;
	struct {
		u_int data_len;
		char *data_val;
	} data;
};
typedef struct writeargs writeargs;
bool_t xdr_writeargs(XDR *, writeargs *);


struct createargs {
	diropargs where;
	sattr attributes;
};
typedef struct createargs createargs;
bool_t xdr_createargs(XDR *, createargs *);


struct renameargs {
	diropargs from;
	diropargs to;
};
typedef struct renameargs renameargs;
bool_t xdr_renameargs(XDR *, renameargs *);


struct linkargs {
	nfs_fh from;
	diropargs to;
};
typedef struct linkargs linkargs;
bool_t xdr_linkargs(XDR *, linkargs *);


struct symlinkargs {
	diropargs from;
	nfspath to;
	sattr attributes;
};
typedef struct symlinkargs symlinkargs;
bool_t xdr_symlinkargs(XDR *, symlinkargs *);


typedef char nfscookie[NFS_COOKIESIZE];
/* static bool_t xdr_nfscookie(XDR *, nfscookie *); */


struct readdirargs {
	nfs_fh dir;
	nfscookie cookie;
	u_int count;
};
typedef struct readdirargs readdirargs;
bool_t xdr_readdirargs(XDR *, readdirargs *);


struct entry {
	u_int fileid;
	filename name;
	nfscookie cookie;
	struct entry *nextentry;
};
typedef struct entry entry;
/* static bool_t xdr_entry(XDR *, entry *); */


struct dirlist {
	entry *entries;
	bool_t eof;
};
typedef struct dirlist dirlist;
/* static bool_t xdr_dirlist(XDR *, dirlist *); */


struct readdirres {
	nfsstat status;
	union {
		dirlist reply;
	} readdirres_u;
};
typedef struct readdirres readdirres;
bool_t xdr_readdirres(XDR *, readdirres *);


struct statfsokres {
	u_int tsize;
	u_int bsize;
	u_int blocks;
	u_int bfree;
	u_int bavail;
};
typedef struct statfsokres statfsokres;
bool_t xdr_statfsokres(XDR *, statfsokres *);


struct statfsres {
	nfsstat status;
	union {
		statfsokres reply;
	} statfsres_u;
};
typedef struct statfsres statfsres;
bool_t xdr_statfsres(XDR *, statfsres *);


#define NFS_PROGRAM ((u_long)100003)
#define NFS_VERSION ((u_long)2)

/* Undef the version 3 ones, and define the v2 ones */
#undef NFSPROC_NULL
#define NFSPROC_NULL ((u_long)0)
#undef NFSPROC_GETATTR
#define NFSPROC_GETATTR ((u_long)1)
#undef NFSPROC_SETATTR
#define NFSPROC_SETATTR ((u_long)2)
#undef NFSPROC_ROOT
#define NFSPROC_ROOT ((u_long)3)
#undef NFSPROC_LOOKUP
#define NFSPROC_LOOKUP ((u_long)4)
#undef NFSPROC_READLINK
#define NFSPROC_READLINK ((u_long)5)
#undef NFSPROC_READ
#define NFSPROC_READ ((u_long)6)
#undef NFSPROC_WRITECACHE
#define NFSPROC_WRITECACHE ((u_long)7)
#undef NFSPROC_WRITE
#define NFSPROC_WRITE ((u_long)8)
#undef NFSPROC_CREATE
#define NFSPROC_CREATE ((u_long)9)
#undef NFSPROC_REMOVE
#define NFSPROC_REMOVE ((u_long)10)
#undef NFSPROC_RENAME
#define NFSPROC_RENAME ((u_long)11)
#undef NFSPROC_LINK
#define NFSPROC_LINK ((u_long)12)
#undef NFSPROC_SYMLINK
#define NFSPROC_SYMLINK ((u_long)13)
#undef NFSPROC_MKDIR
#define NFSPROC_MKDIR ((u_long)14)
#undef NFSPROC_RMDIR
#define NFSPROC_RMDIR ((u_long)15)
#undef NFSPROC_READDIR
#define NFSPROC_READDIR ((u_long)16)
#undef NFSPROC_STATFS
#define NFSPROC_STATFS ((u_long)17)

extern void *nfsproc_null_2(void *, struct svc_req *);
extern attrstat *nfsproc_getattr_2(nfs_fh *, struct svc_req *);
extern attrstat *nfsproc_setattr_2(sattrargs *, struct svc_req *);
extern void *nfsproc_root_2(void *, struct svc_req *);
extern diropres *nfsproc_lookup_2(diropargs *, struct svc_req *);
extern readlinkres *nfsproc_readlink_2(nfs_fh *, struct svc_req *);
extern readres *nfsproc_read_2(readargs *, struct svc_req *);
extern void *nfsproc_writecache_2(void *, struct svc_req *);
extern attrstat *nfsproc_write_2(writeargs *, struct svc_req *);
extern diropres *nfsproc_create_2(createargs *, struct svc_req *);
extern nfsstat *nfsproc_remove_2(diropargs *, struct svc_req *);
extern nfsstat *nfsproc_rename_2(renameargs *, struct svc_req *);
extern nfsstat *nfsproc_link_2(linkargs *, struct svc_req *);
extern nfsstat *nfsproc_symlink_2(symlinkargs *, struct svc_req *);
extern diropres *nfsproc_mkdir_2(createargs *, struct svc_req *);
extern nfsstat *nfsproc_rmdir_2(diropargs *, struct svc_req *);
extern readdirres *nfsproc_readdir_2(readdirargs *, struct svc_req *);
extern statfsres *nfsproc_statfs_2(nfs_fh *, struct svc_req *);
