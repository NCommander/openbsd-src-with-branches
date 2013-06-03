/*
 * Copyright (c) 2013 Sylvestre Gallon <ccna.syl@gmail.com>
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

#ifndef _FUSE_H_
#define _FUSE_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

#include <fcntl.h>
#include <utime.h>

#ifdef __cplusplus
extern "C" {
#endif

struct fuse_chan;
struct fuse_args;
struct fuse_session;

struct fuse_file_info {
	int32_t		 flags;		/* open(2) flags */
	uint32_t	 fh_old;	/* old file handle */
	int32_t		 writepage;
	uint32_t	 direct_io:1;
	uint32_t	 keep_cache:1;
	uint32_t	 flush:1;
	uint32_t	 __padd:29;
	uint64_t	 fh;		/* file handle */
	uint64_t	 lock_owner;
};

struct fuse_conn_info {
	uint32_t proto_major;
	uint32_t proto_minor;
	uint32_t async_read;
	uint32_t max_write;
	uint32_t max_readahead;
	uint32_t reserved[27];
};

typedef ino_t fuse_ino_t;
typedef int (*fuse_fill_dir_t)(void *, const char *, const struct stat *,
    off_t);

typedef struct fuse_dirhandle {
	fuse_fill_dir_t filler;
	void *buf;
	int filled;
	int full;
	int isgetdir;
	uint32_t size;
	uint32_t start;
	uint32_t idx;
} *fuse_dirh_t;

typedef int (*fuse_dirfil_t)(fuse_dirh_t, const char *, int, ino_t);

struct fuse_operations {
	int	(*getattr)(const char *, struct stat *);
	int	(*readlink)(const char *, char *, size_t);
	int	(*getdir)(const char *, fuse_dirh_t, fuse_dirfil_t);
	int	(*mknod)(const char *, mode_t, dev_t);
	int	(*mkdir)(const char *, mode_t);
	int	(*unlink)(const char *);
	int	(*rmdir)(const char *);
	int	(*symlink)(const char *, const char *);
	int	(*rename)(const char *, const char *);
	int	(*link)(const char *, const char *);
	int	(*chmod)(const char *, mode_t);
	int	(*chown)(const char *, uid_t, gid_t);
	int	(*truncate)(const char *, off_t);
	int	(*utime)(const char *, struct utimbuf *);
	int	(*open)(const char *, struct fuse_file_info *);
	int	(*read)(const char *, char *, size_t, off_t,
		struct fuse_file_info *);
	int	(*write)(const char *, const char *, size_t, off_t,
		struct fuse_file_info *);
	int	(*statfs)(const char *, struct statvfs *);
	int	(*flush)(const char *, struct fuse_file_info *);
	int	(*release)(const char *, struct fuse_file_info *);
	int	(*fsync)(const char *, int, struct fuse_file_info *);
	int	(*setxattr)(const char *, const char *, const char *, size_t,
		int);
	int	(*getxattr)(const char *, const char *, char *, size_t);
	int	(*listxattr)(const char *, char *, size_t);
	int	(*removexattr)(const char *, const char *);
	int	(*opendir)(const char *, struct fuse_file_info *);
	int	(*readdir)(const char *, void *, fuse_fill_dir_t, off_t,
		struct fuse_file_info *);
	int	(*releasedir)(const char *, struct fuse_file_info *);
	int	(*fsyncdir)(const char *, int, struct fuse_file_info *);
	void	*(*init)(struct fuse_conn_info *);
	void	(*destroy)(void *);
	int	(*access)(const char *, int);
	int	(*create)(const char *, mode_t, struct fuse_file_info *);
	int	(*ftruncate)(const char *, off_t, struct fuse_file_info *);
	int	(*fgetattr)(const char *, struct stat *, struct fuse_file_info *);
	int	(*lock)(const char *, struct fuse_file_info *, int, struct flock *);
	int	(*utimens)(const char *, const struct timespec *);
	int	(*bmap)(const char *, size_t , uint64_t *);
};

#ifndef FUSE_USE_VERSION
#define FUSE_USE_VERSION 26
#endif

#if FUSE_USE_VERSION >= 26
#define FUSE_VERSION 26
#else
#error "Fuse version < 26 not supported"
#endif


/*
 * API prototypes
 */
int fuse_main(int, char **, const struct fuse_operations *, void *);
struct fuse *fuse_new(struct fuse_chan *, struct fuse_args *,
    const struct fuse_operations *, size_t, void *);
int fuse_parse_cmdline(struct fuse_args *, char **, int *, int *);
struct fuse_chan *fuse_mount(const char *, struct fuse_args *);
void fuse_remove_signal_handlers(struct fuse_session *);
int fuse_set_signal_handlers(struct fuse_session *);
struct fuse_session *fuse_get_session(struct fuse *);
int fuse_is_lib_option(const char *);
int fuse_loop(struct fuse *);
int fuse_loop_mt(struct fuse *);
int fuse_chan_fd(struct fuse_chan *);
void fuse_unmount(const char *, struct fuse_chan *);
int fuse_daemonize(int);
void fuse_destroy(struct fuse *);

#ifdef __cplusplus
}
#endif

#endif /* _FUSE_H_ */
