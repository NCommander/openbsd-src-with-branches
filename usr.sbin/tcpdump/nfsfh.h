/*	$NetBSD: nfsfh.h,v 1.2 1995/03/06 19:10:39 mycroft Exp $	*/

/*
 * Header: nfsfh.h,v 1.3 94/06/12 14:32:58 leres Exp
 *
 * nfsfh.h - NFS file handle definitions (for portable use)
 *
 * Jeffrey C. Mogul
 * Digital Equipment Corporation
 * Western Research Laboratory
 */

/*
 * Internal representation of dev_t, because different NFS servers
 * that we might be spying upon use different external representations.
 */
typedef struct {
	u_long	Minor;	/* upper case to avoid clashing with macro names */
	u_long	Major;
} my_devt;

#define	dev_eq(a,b)	((a.Minor == b.Minor) && (a.Major == b.Major))

/*
 * Many file servers now use a large file system ID.  This is
 * our internal representation of that.
 */
typedef	struct {
	my_devt	fsid_dev;
	u_long	fsid_code;
} my_fsid;

#define	fsid_eq(a,b)	((a.fsid_code == b.fsid_code) &&\
			 dev_eq(a.fsid_dev, b.fsid_dev))

extern void Parse_fh(caddr_t *, my_fsid *, ino_t *, char **, char **, int);
