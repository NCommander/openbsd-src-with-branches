/*	$OpenBSD: nfs_bio.c,v 1.32.2.1 2002/01/31 22:55:47 niklas Exp $	*/
/*	$NetBSD: nfs_bio.c,v 1.25.4.2 1996/07/08 20:47:04 jtc Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)nfs_bio.c	8.9 (Berkeley) 3/30/95
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/kernel.h>
#include <sys/namei.h>
#include <sys/pool.h>

#include <uvm/uvm.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#include <nfs/nfsmount.h>
#include <nfs/nfsnode.h>
#include <nfs/nfs_var.h>

extern struct proc *nfs_iodwant[NFS_MAXASYNCDAEMON];
extern int nfs_numasync;
struct nfsstats nfsstats;

/*
 * Vnode op for read using bio
 * Any similarity to readip() is purely coincidental
 */
int
nfs_bioread(vp, uio, ioflag, cred)
	struct vnode *vp;
	struct uio *uio;
	int ioflag;
	struct ucred *cred;
{
	struct nfsnode *np = VTONFS(vp);
	int biosize;
	struct buf *bp = NULL;
	struct vattr vattr;
	struct proc *p;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	caddr_t baddr;
	int got_buf = 0, error = 0, n = 0, on = 0;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_READ)
		panic("nfs_read mode");
#endif
	if (uio->uio_resid == 0)
		return (0);
	if (uio->uio_offset < 0)
		return (EINVAL);
	p = uio->uio_procp;
	if ((nmp->nm_flag & (NFSMNT_NFSV3 | NFSMNT_GOTFSINFO)) == NFSMNT_NFSV3)
		(void)nfs_fsinfo(nmp, vp, cred, p);
	biosize = nmp->nm_rsize;
	/*
	 * For nfs, cache consistency can only be maintained approximately.
	 * Although RFC1094 does not specify the criteria, the following is
	 * believed to be compatible with the reference port.
	 * For nfs:
	 * If the file's modify time on the server has changed since the
	 * last read rpc or you have written to the file,
	 * you may have lost data cache consistency with the
	 * server, so flush all of the file's data out of the cache.
	 * Then force a getattr rpc to ensure that you have up to date
	 * attributes.
	 * NB: This implies that cache data can be read when up to
	 * NFS_ATTRTIMEO seconds out of date. If you find that you need current
	 * attributes this could be forced by setting n_attrstamp to 0 before
	 * the VOP_GETATTR() call.
	 */
	/* 
	 * There is no way to modify a symbolic link via NFS or via
	 * VFS, so we don't check if the link was modified 
	 */
	if (vp->v_type != VLNK) {
		if (np->n_flag & NMODIFIED) {
			np->n_attrstamp = 0;
			error = VOP_GETATTR(vp, &vattr, cred, p);
			if (error)
				return (error);
			np->n_mtime = vattr.va_mtime.tv_sec;
		} else {
			error = VOP_GETATTR(vp, &vattr, cred, p);
			if (error)
				return (error);
			if (np->n_mtime != vattr.va_mtime.tv_sec) {
				error = nfs_vinvalbuf(vp, V_SAVE, cred, p, 1);
				if (error)
					return (error);
				np->n_mtime = vattr.va_mtime.tv_sec;
			}
		}
	}

	/*
	 * update the cached read creds for this vnode.
	 */
	if (np->n_rcred){
		crfree(np->n_rcred);
	}
	np->n_rcred = cred;
	crhold(cred);

	do {
	    if ((vp->v_flag & VROOT) && vp->v_type == VLNK) {
		    return (nfs_readlinkrpc(vp, uio, cred));
	    }
	    baddr = (caddr_t)0;
	    switch (vp->v_type) {
	    case VREG:
		nfsstats.biocache_reads++;

		error = 0;
		if (uio->uio_offset >= np->n_size) {
			break;
		}
		while (uio->uio_resid > 0) {
			void *win;
			vsize_t bytelen = MIN(np->n_size - uio->uio_offset,
					      uio->uio_resid);

			if (bytelen == 0)
				break;
			win = ubc_alloc(&vp->v_uobj, uio->uio_offset,
					&bytelen, UBC_READ);
			error = uiomove(win, bytelen, uio);
			ubc_release(win, 0);
			if (error) {
				break;
			}
		}
		n = 0;
		break;

	    case VLNK:
		nfsstats.biocache_readlinks++;
		bp = nfs_getcacheblk(vp, (daddr_t)0, NFS_MAXPATHLEN, p);
		if (!bp)
			return (EINTR);
		if ((bp->b_flags & B_DONE) == 0) {
			bp->b_flags |= B_READ;
			error = nfs_doio(bp, p);
			if (error) {
				brelse(bp);
				return (error);
			}
		}
		n = MIN(uio->uio_resid, NFS_MAXPATHLEN - bp->b_resid);
		got_buf = 1;
		on = 0;
		break;
	    default:
		printf(" nfsbioread: type %x unexpected\n",vp->v_type);
		break;
	    }

	    if (n > 0) {
		if (!baddr)
			baddr = bp->b_data;
		error = uiomove(baddr + on, (int)n, uio);
	    }
	    switch (vp->v_type) {
	    case VREG:
		break;
	    case VLNK:
		n = 0;
		break;
	    default:
		printf(" nfsbioread: type %x unexpected\n",vp->v_type);
	    }
	    if (got_buf)
		brelse(bp);
	} while (error == 0 && uio->uio_resid > 0 && n > 0);
	return (error);
}

/*
 * Vnode op for write using bio
 */
int
nfs_write(v)
	void *v;
{
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap = v;
	struct uio *uio = ap->a_uio;
	struct proc *p = uio->uio_procp;
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	struct ucred *cred = ap->a_cred;
	int ioflag = ap->a_ioflag;
	struct vattr vattr;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	void *win;
	voff_t oldoff, origoff;
	vsize_t bytelen;
	int error = 0;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_WRITE)
		panic("nfs_write mode");
	if (uio->uio_segflg == UIO_USERSPACE && uio->uio_procp != curproc)
		panic("nfs_write proc");
#endif
	if (vp->v_type != VREG)
		return (EIO);
	if (np->n_flag & NWRITEERR) {
		np->n_flag &= ~NWRITEERR;
		return (np->n_error);
	}
	if ((nmp->nm_flag & (NFSMNT_NFSV3 | NFSMNT_GOTFSINFO)) == NFSMNT_NFSV3)
		(void)nfs_fsinfo(nmp, vp, cred, p);
	if (ioflag & (IO_APPEND | IO_SYNC)) {
		if (np->n_flag & NMODIFIED) {
			np->n_attrstamp = 0;
			error = nfs_vinvalbuf(vp, V_SAVE, cred, p, 1);
			if (error)
				return (error);
		}
		if (ioflag & IO_APPEND) {
			np->n_attrstamp = 0;
			error = VOP_GETATTR(vp, &vattr, cred, p);
			if (error)
				return (error);
			uio->uio_offset = np->n_size;
		}
	}
	if (uio->uio_offset < 0)
		return (EINVAL);
	if (uio->uio_resid == 0)
		return (0);
	/*
	 * Maybe this should be above the vnode op call, but so long as
	 * file servers have no limits, i don't think it matters
	 */
	if (p && uio->uio_offset + uio->uio_resid >
	      p->p_rlimit[RLIMIT_FSIZE].rlim_cur) {
		psignal(p, SIGXFSZ);
		return (EFBIG);
	}

	/*
	 * update the cached write creds for this node.
	 */
	if (np->n_wcred) {
		crfree(np->n_wcred);
	}
	np->n_wcred = cred;
	crhold(cred);

	origoff = uio->uio_offset;
	do {
		oldoff = uio->uio_offset;
		/*
		 * XXXART - workaround for compiler bug on 68k. Wieee!
		 */
		*((volatile vsize_t *)&bytelen) = uio->uio_resid;

		nfsstats.biocache_writes++;

		np->n_flag |= NMODIFIED;
		if (np->n_size < uio->uio_offset + bytelen) {
			np->n_size = uio->uio_offset + bytelen;
		}
		if ((uio->uio_offset & PAGE_MASK) == 0 &&
		    ((uio->uio_offset + bytelen) & PAGE_MASK) == 0) {
			win = ubc_alloc(&vp->v_uobj, uio->uio_offset, &bytelen,
			    UBC_WRITE | UBC_FAULTBUSY);
		} else {
			win = ubc_alloc(&vp->v_uobj, uio->uio_offset, &bytelen,
			    UBC_WRITE);
		}
		error = uiomove(win, bytelen, uio);
		ubc_release(win, 0);
		if (error) {
			break;
		}

		/*
		 * update UVM's notion of the size now that we've
		 * copied the data into the vnode's pages.
		 */

		if (vp->v_size < uio->uio_offset) {
			uvm_vnp_setsize(vp, uio->uio_offset);
		}

		if ((oldoff & ~(nmp->nm_wsize - 1)) !=
		    (uio->uio_offset & ~(nmp->nm_wsize - 1))) {
			simple_lock(&vp->v_interlock);
			error = VOP_PUTPAGES(vp,
			    trunc_page(oldoff & ~(nmp->nm_wsize - 1)),
			    round_page((uio->uio_offset + nmp->nm_wsize - 1) &
				       ~(nmp->nm_wsize - 1)),
			    PGO_CLEANIT | PGO_WEAK);
		}
	} while (uio->uio_resid > 0);
	if ((ioflag & IO_SYNC)) {
		simple_lock(&vp->v_interlock);
		error = VOP_PUTPAGES(vp,
		    trunc_page(origoff & ~(nmp->nm_wsize - 1)),
		    round_page((uio->uio_offset + nmp->nm_wsize - 1) &
			       ~(nmp->nm_wsize - 1)),
		    PGO_CLEANIT | PGO_SYNCIO);
	}
	return error;
}

/*
 * Get an nfs cache block.
 * Allocate a new one if the block isn't currently in the cache
 * and return the block marked busy. If the calling process is
 * interrupted by a signal for an interruptible mount point, return
 * NULL.
 */
struct buf *
nfs_getcacheblk(vp, bn, size, p)
	struct vnode *vp;
	daddr_t bn;
	int size;
	struct proc *p;
{
	struct buf *bp;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);

	if (nmp->nm_flag & NFSMNT_INT) {
		bp = getblk(vp, bn, size, PCATCH, 0);
		while (bp == NULL) {
			if (nfs_sigintr(nmp, NULL, p))
				return (NULL);
			bp = getblk(vp, bn, size, 0, 2 * hz);
		}
	} else
		bp = getblk(vp, bn, size, 0, 0);
	return (bp);
}

/*
 * Flush and invalidate all dirty buffers. If another process is already
 * doing the flush, just wait for completion.
 */
int
nfs_vinvalbuf(vp, flags, cred, p, intrflg)
	struct vnode *vp;
	int flags;
	struct ucred *cred;
	struct proc *p;
	int intrflg;
{
	struct nfsnode *np = VTONFS(vp);
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	int error = 0, slpflag, slptimeo;

	if ((nmp->nm_flag & NFSMNT_INT) == 0)
		intrflg = 0;
	if (intrflg) {
		slpflag = PCATCH;
		slptimeo = 2 * hz;
	} else {
		slpflag = 0;
		slptimeo = 0;
	}
	/*
	 * First wait for any other process doing a flush to complete.
	 */
	while (np->n_flag & NFLUSHINPROG) {
		np->n_flag |= NFLUSHWANT;
		error = tsleep((caddr_t)&np->n_flag, PRIBIO + 2, "nfsvinval",
			slptimeo);
		if (error && intrflg && nfs_sigintr(nmp, NULL, p))
			return (EINTR);
	}

	/*
	 * Now, flush as required.
	 */
	np->n_flag |= NFLUSHINPROG;
	error = vinvalbuf(vp, flags, cred, p, slpflag, 0);
	while (error) {
		if (intrflg && nfs_sigintr(nmp, NULL, p)) {
			np->n_flag &= ~NFLUSHINPROG;
			if (np->n_flag & NFLUSHWANT) {
				np->n_flag &= ~NFLUSHWANT;
				wakeup((caddr_t)&np->n_flag);
			}
			return (EINTR);
		}
		error = vinvalbuf(vp, flags, cred, p, 0, slptimeo);
	}
	np->n_flag &= ~(NMODIFIED | NFLUSHINPROG);
	if (np->n_flag & NFLUSHWANT) {
		np->n_flag &= ~NFLUSHWANT;
		wakeup((caddr_t)&np->n_flag);
	}
	return (0);
}

/*
 * Initiate asynchronous I/O. Return an error if no nfsiods are available.
 * This is mainly to avoid queueing async I/O requests when the nfsiods
 * are all hung on a dead server.
 */
int
nfs_asyncio(bp)
	struct buf *bp;
{
	int i;

	if (nfs_numasync == 0)
		return (EIO);
	for (i = 0; i < NFS_MAXASYNCDAEMON; i++) {
	    if (nfs_iodwant[i]) {
		TAILQ_INSERT_TAIL(&nfs_bufq, bp, b_freelist);
		nfs_iodwant[i] = NULL;
		wakeup((caddr_t)&nfs_iodwant[i]);
		return (0);
	    }
	}

	return (EIO);
}

/*
 * Do an I/O operation to/from a cache block. This may be called
 * synchronously or from an nfsiod.
 */
int
nfs_doio(bp, p)
	struct buf *bp;
	struct proc *p;
{
	struct uio *uiop;
	struct vnode *vp;
	struct nfsnode *np;
	struct nfsmount *nmp;
	int error = 0, diff, len, iomode, must_commit = 0;
	struct uio uio;
	struct iovec io;

	vp = bp->b_vp;
	np = VTONFS(vp);
	nmp = VFSTONFS(vp->v_mount);
	uiop = &uio;
	uiop->uio_iov = &io;
	uiop->uio_iovcnt = 1;
	uiop->uio_segflg = UIO_SYSSPACE;
	uiop->uio_procp = p;

	/*
	 * Historically, paging was done with physio, but no more...
	 */
	if (bp->b_flags & B_PHYS) {
	    /*
	     * ...though reading /dev/drum still gets us here.
	     */
	    io.iov_len = uiop->uio_resid = bp->b_bcount;
	    /* mapping was done by vmapbuf() */
	    io.iov_base = bp->b_data;
	    uiop->uio_offset = ((off_t)bp->b_blkno) << DEV_BSHIFT;
	    if (bp->b_flags & B_READ) {
		uiop->uio_rw = UIO_READ;
		nfsstats.read_physios++;
		error = nfs_readrpc(vp, uiop);
	    } else {
		iomode = NFSV3WRITE_DATASYNC;
		uiop->uio_rw = UIO_WRITE;
		nfsstats.write_physios++;
		error = nfs_writerpc(vp, uiop, &iomode, &must_commit);
	    }
	    if (error) {
		bp->b_flags |= B_ERROR;
		bp->b_error = error;
	    }
	} else if (bp->b_flags & B_READ) {
	    io.iov_len = uiop->uio_resid = bp->b_bcount;
	    io.iov_base = bp->b_data;
	    uiop->uio_rw = UIO_READ;
	    switch (vp->v_type) {
	    case VREG:
		uiop->uio_offset = ((off_t)bp->b_blkno) << DEV_BSHIFT;
		nfsstats.read_bios++;
		error = nfs_readrpc(vp, uiop);
		if (!error && uiop->uio_resid) {
			/*
			 * If len > 0, there is a hole in the file and
			 * no writes after the hole have been pushed to
			 * the server yet.
			 * Just zero fill the rest of the valid area.
			 */
			diff = bp->b_bcount - uiop->uio_resid;
			len = np->n_size - ((((off_t)bp->b_blkno) << DEV_BSHIFT)
				+ diff);
			if (len > 0) {
				len = MIN(len, uiop->uio_resid);
				memset((char *)bp->b_data + diff, 0, len);
			}
		}
		if (p && (vp->v_flag & VTEXT) &&
		    (np->n_mtime != np->n_vattr.va_mtime.tv_sec)) {
			uprintf("Process killed due to text file modification\n");
			psignal(p, SIGKILL);
			p->p_holdcnt++;
		}
		break;
	    case VLNK:
		uiop->uio_offset = (off_t)0;
		nfsstats.readlink_bios++;
		error = nfs_readlinkrpc(vp, uiop, curproc->p_ucred);
		break;
	    default:
		printf("nfs_doio:  type %x unexpected\n",vp->v_type);
		break;
	    }
	    if (error) {
		bp->b_flags |= B_ERROR;
		bp->b_error = error;
	    }
	} else {
	    io.iov_base = bp->b_data;
	    io.iov_len = uiop->uio_resid = bp->b_bcount;
	    uiop->uio_offset = ((off_t)bp->b_blkno) << DEV_BSHIFT;
	    uiop->uio_rw = UIO_WRITE;
	    nfsstats.write_bios++;
	    iomode = NFSV3WRITE_UNSTABLE;
	    error = nfs_writerpc(vp, uiop, &iomode, &must_commit);
	}
	bp->b_resid = uiop->uio_resid;
	if (must_commit)
		nfs_clearcommit(vp->v_mount);
	biodone(bp);
	return (error);
}

/*
 * Vnode op for VM getpages.
 */
int
nfs_getpages(v)
	void *v;
{
	struct vop_getpages_args /* {
		struct vnode *a_vp;
		voff_t a_offset;
		vm_page_t *a_m;
		int *a_count;
		int a_centeridx;
		vm_prot_t a_access_type;
		int a_advice;
		int a_flags;
	} */ *ap = v;

	struct vnode *vp = ap->a_vp;
#if defined(LOCKDEBUG) || defined(MULTIPROCESSOR)
	struct uvm_object *uobj = &vp->v_uobj;
#endif
	struct nfsnode *np = VTONFS(vp);
	struct vm_page *pg, **pgs;
	struct proc *p = curproc;
	off_t origoffset;
	int i, error, npages;
	boolean_t v3 = NFS_ISV3(vp);
	boolean_t write = (ap->a_access_type & VM_PROT_WRITE) != 0;
	UVMHIST_FUNC("nfs_getpages"); UVMHIST_CALLED(ubchist);

	/*
	 * update the cached read creds for this node.
	 */

	if (np->n_rcred) {
		crfree(np->n_rcred);
	}
	np->n_rcred = curproc->p_ucred;
	crhold(np->n_rcred);

	/*
	 * call the genfs code to get the pages.
	 */

	npages = *ap->a_count;
	error = genfs_getpages(v);
	if (error || !write || !v3) {
		return error;
	}

	/*
	 * this is a write fault, update the commit info.
	 */

	origoffset = ap->a_offset;
	pgs = ap->a_m;

	lockmgr(&np->n_commitlock, LK_EXCLUSIVE, NULL, p);
	nfs_del_committed_range(vp, origoffset, npages);
	nfs_del_tobecommitted_range(vp, origoffset, npages);
	simple_lock(&uobj->vmobjlock);
	for (i = 0; i < npages; i++) {
		pg = pgs[i];
		if (pg == NULL || pg == PGO_DONTCARE) {
			continue;
		}
		pg->flags &= ~(PG_NEEDCOMMIT|PG_RDONLY);
	}
	simple_unlock(&uobj->vmobjlock);
	lockmgr(&np->n_commitlock, LK_RELEASE, NULL, p);
	return 0;
}

int
nfs_gop_write(struct vnode *vp, struct vm_page **pgs, int npages, int flags)
{
#if defined(LOCKDEBUG) || defined(MULTIPROCESSOR)
	struct uvm_object *uobj = &vp->v_uobj;
#endif
	struct nfsnode *np = VTONFS(vp);
	struct proc *p = curproc;
	off_t origoffset, commitoff;
	uint32_t commitbytes;
	int error, i;
	int bytes;
	boolean_t v3 = NFS_ISV3(vp);
	boolean_t weak = flags & PGO_WEAK;
	UVMHIST_FUNC("nfs_gop_write"); UVMHIST_CALLED(ubchist);

	/* XXX for now, skip the v3 stuff. */
	v3 = FALSE;

	/*
	 * for NFSv2, just write normally.
	 */

	if (!v3) {
		return genfs_gop_write(vp, pgs, npages, flags);
	}

	/*
	 * for NFSv3, use delayed writes and the "commit" operation
	 * to avoid sync writes.
	 */

	origoffset = pgs[0]->offset;
	bytes = npages << PAGE_SHIFT;
	lockmgr(&np->n_commitlock, LK_EXCLUSIVE, NULL, p);
	if (nfs_in_committed_range(vp, origoffset, bytes)) {
		goto committed;
	}
	if (nfs_in_tobecommitted_range(vp, origoffset, bytes)) {
		if (weak) {
			lockmgr(&np->n_commitlock, LK_RELEASE, NULL, p);
			return 0;
		} else {
			commitoff = np->n_pushlo;
			commitbytes = (uint32_t)(np->n_pushhi - np->n_pushlo);
			goto commit;
		}
	} else {
		commitoff = origoffset;
		commitbytes = npages << PAGE_SHIFT;
	}
	simple_lock(&uobj->vmobjlock);
	for (i = 0; i < npages; i++) {
		pgs[i]->flags |= PG_NEEDCOMMIT|PG_RDONLY;
		pgs[i]->flags &= ~PG_CLEAN;
	}
	simple_unlock(&uobj->vmobjlock);
	lockmgr(&np->n_commitlock, LK_RELEASE, NULL, p);
	error = genfs_gop_write(vp, pgs, npages, flags);
	if (error) {
		return error;
	}
	lockmgr(&np->n_commitlock, LK_EXCLUSIVE, NULL, p);
	if (weak) {
		nfs_add_tobecommitted_range(vp, origoffset,
		    npages << PAGE_SHIFT);
	} else {
commit:
		error = nfs_commit(vp, commitoff, commitbytes, p);
		nfs_del_tobecommitted_range(vp, commitoff, commitbytes);
committed:
		simple_lock(&uobj->vmobjlock);
		for (i = 0; i < npages; i++) {
			pgs[i]->flags &= ~(PG_NEEDCOMMIT|PG_RDONLY);
		}
		simple_unlock(&uobj->vmobjlock);
	}
	lockmgr(&np->n_commitlock, LK_RELEASE, NULL, p);
	return error;
}
