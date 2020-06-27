/*	$OpenBSD: pmap.c,v 1.19 2020/06/26 20:58:38 kettenis Exp $ */

/*
 * Copyright (c) 2015 Martin Pieuchot
 * Copyright (c) 2001, 2002, 2007 Dale Rahn.
 * All rights reserved.
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 */

/*
 * Copyright (c) 2020 Mark Kettenis <kettenis@openbsd.org>
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

#include <sys/param.h>
#include <sys/atomic.h>
#include <sys/pool.h>

#include <uvm/uvm_extern.h>

#include <machine/cpufunc.h>
#include <machine/pmap.h>
#include <machine/pte.h>

#include <dev/ofw/fdt.h>

extern char _start[], _etext[], _end[];

#define	PMAP_HASH_LOCK_INIT()		/* nothing */
#define	PMAP_HASH_LOCK(s)		(void)s
#define	PMAP_HASH_UNLOCK(s)		/* nothing */

#define	PMAP_VP_LOCK_INIT(pm)		/* nothing */
#define	PMAP_VP_LOCK(pm)		/* nothing */
#define	PMAP_VP_UNLOCK(pm)		/* nothing */
#define	PMAP_VP_ASSERT_LOCKED(pm)	/* nothing */

struct pmap kernel_pmap_store;

struct pte *pmap_ptable;
int	pmap_ptab_cnt;
uint64_t pmap_ptab_mask;

#define HTABMEMSZ	(pmap_ptab_cnt * 8 * sizeof(struct pte))
#define HTABSIZE	(ffs(pmap_ptab_cnt) - 12)

struct pate *pmap_pat;

#define PATMEMSZ	(64 * 1024)
#define PATSIZE		(ffs(PATMEMSZ) - 12)

struct pte_desc {
	/* Linked list of phys -> virt entries */
	LIST_ENTRY(pte_desc) pted_pv_list;
	struct pte pted_pte;
	pmap_t pted_pmap;
	vaddr_t pted_va;
	uint64_t pted_vsid;
};

#define PTED_VA_PTEGIDX_M	0x07
#define PTED_VA_HID_M		0x08
#define PTED_VA_MANAGED_M	0x10
#define PTED_VA_WIRED_M		0x20
#define PTED_VA_EXEC_M		0x40

void	pmap_pted_syncicache(struct pte_desc *);

struct slb_desc {
	LIST_ENTRY(slb_desc) slbd_list;
	uint64_t	slbd_esid;
	uint64_t	slbd_vsid;
	struct pmapvp1	*slbd_vp;
};

struct slb_desc	kernel_slb_desc[32];

struct slb_desc *pmap_slbd_lookup(pmap_t, vaddr_t);

struct pmapvp1 {
	struct pmapvp2 *vp[VP_IDX1_CNT];
};

struct pmapvp2 {
	struct pte_desc *vp[VP_IDX2_CNT];
};

CTASSERT(sizeof(struct pmapvp1) == sizeof(struct pmapvp2));

static inline int
VP_IDX1(vaddr_t va)
{
	return (va >> VP_IDX1_POS) & VP_IDX1_MASK;
}

static inline int
VP_IDX2(vaddr_t va)
{
	return (va >> VP_IDX2_POS) & VP_IDX2_MASK;
}

void	pmap_vp_destroy(pmap_t);
void	pmap_release(pmap_t);

struct pool pmap_pmap_pool;
struct pool pmap_vp_pool;
struct pool pmap_pted_pool;
struct pool pmap_slbd_pool;

int pmap_initialized = 0;

/*
 * We use only 4K pages and 256MB segments.  That means p = b = 12 and
 * s = 28.
 */

#define KERNEL_VSID_BIT		0x0000001000000000ULL
#define VSID_HASH_MASK		0x0000007fffffffffULL

static inline int
PTED_HID(struct pte_desc *pted)
{
	return !!(pted->pted_va & PTED_VA_HID_M);
}

static inline int
PTED_PTEGIDX(struct pte_desc *pted)
{
	return (pted->pted_va & PTED_VA_PTEGIDX_M);
}

static inline int
PTED_MANAGED(struct pte_desc *pted)
{
	return !!(pted->pted_va & PTED_VA_MANAGED_M);
}

static inline int
PTED_VALID(struct pte_desc *pted)
{
	return !!(pted->pted_pte.pte_hi & PTE_VALID);
}

#define TLBIEL_MAX_SETS		4096
#define TLBIEL_SET_SHIFT	12
#define TLBIEL_INVAL_SET	(0x3 << 10)

void
tlbia(void)
{
	int set;

	for (set = 0; set < TLBIEL_MAX_SETS; set++)
		tlbiel((set << TLBIEL_SET_SHIFT) | TLBIEL_INVAL_SET);
}

/*
 * Return AVA for use with TLB invalidate instructions.
 */
static inline uint64_t
pmap_ava(uint64_t vsid, vaddr_t va)
{
	return ((vsid << ADDR_VSID_SHIFT) | (va & ADDR_PIDX));
}

/*
 * Return AVA for a PTE descriptor.
 */
static inline uint64_t
pmap_pted2ava(struct pte_desc *pted)
{
	return pmap_ava(pted->pted_vsid, pted->pted_va);
}

/*
 * Return the top 64 bits of the (80-bit) VPN for a PTE descriptor.
 */
static inline uint64_t
pmap_pted2avpn(struct pte_desc *pted)
{
	return (pted->pted_vsid << (PTE_VSID_SHIFT) |
	    (pted->pted_va & ADDR_PIDX) >>
		(ADDR_VSID_SHIFT - PTE_VSID_SHIFT));
}

static inline u_int
pmap_pte2flags(uint64_t pte_lo)
{
	return (((pte_lo & PTE_REF) ? PG_PMAP_REF : 0) |
	    ((pte_lo & PTE_CHG) ? PG_PMAP_MOD : 0));
}

static inline u_int
pmap_flags2pte(u_int flags)
{
	return (((flags & PG_PMAP_REF) ? PTE_REF : 0) |
	    ((flags & PG_PMAP_MOD) ? PTE_CHG : 0));
}

static inline uint64_t
pmap_kernel_vsid(uint64_t esid)
{
	uint64_t vsid;
	vsid = (((esid << 8) | (esid > 28)) * 0x13bb) & (KERNEL_VSID_BIT - 1);
	return vsid | KERNEL_VSID_BIT;
}

static inline uint64_t
pmap_va2vsid(pmap_t pm, vaddr_t va)
{
	uint64_t esid = va >> ADDR_ESID_SHIFT;
	struct slb_desc *slbd;

	if (pm == pmap_kernel())
		return pmap_kernel_vsid(esid);

	slbd = pmap_slbd_lookup(pm, va);
	if (slbd)
		return slbd->slbd_vsid;

	return 0;
}

void
pmap_attr_save(paddr_t pa, uint64_t bits)
{
	struct vm_page *pg;

	pg = PHYS_TO_VM_PAGE(pa);
	if (pg == NULL)
		return;

	atomic_setbits_int(&pg->pg_flags,  pmap_pte2flags(bits));
}

struct pte *
pmap_ptedinhash(struct pte_desc *pted)
{
	struct pte *pte;
	vaddr_t va;
	uint64_t vsid, hash;
	int idx;

	va = pted->pted_va & ~PAGE_MASK;
	vsid = pted->pted_vsid;
	hash = (vsid & VSID_HASH_MASK) ^ ((va & ADDR_PIDX) >> ADDR_PIDX_SHIFT);
	idx = (hash & pmap_ptab_mask);

	idx ^= (PTED_HID(pted) ? pmap_ptab_mask : 0);
	pte = pmap_ptable + (idx * 8);
	pte += PTED_PTEGIDX(pted); /* increment by index into pteg */

	/*
	 * We now have the pointer to where it will be, if it is
	 * currently mapped. If the mapping was thrown away in
	 * exchange for another page mapping, then this page is not
	 * currently in the hash.
	 */
	if ((pted->pted_pte.pte_hi |
	     (PTED_HID(pted) ? PTE_HID : 0)) == pte->pte_hi)
		return pte;

	return NULL;
}

struct slb_desc *
pmap_slbd_lookup(pmap_t pm, vaddr_t va)
{
	uint64_t esid = va >> ADDR_ESID_SHIFT;
	struct slb_desc *slbd;

	LIST_FOREACH(slbd, &pm->pm_slbd, slbd_list) {
		if (slbd->slbd_esid == esid)
			return slbd;
	}

	return NULL;
}

void
pmap_slbd_cache(pmap_t pm, struct slb_desc *slbd)
{
	uint64_t slbe, slbv;
	int idx;

	for (idx = 0; idx < nitems(pm->pm_slb); idx++) {
		if (pm->pm_slb[idx].slb_slbe == 0)
			break;
	}
	if (idx == nitems(pm->pm_slb))
		idx = arc4random_uniform(nitems(pm->pm_slb));

	slbe = (slbd->slbd_esid << SLBE_ESID_SHIFT) | SLBE_VALID | idx;
	slbv = slbd->slbd_vsid << SLBV_VSID_SHIFT;

	pm->pm_slb[idx].slb_slbe = slbe;
	pm->pm_slb[idx].slb_slbv = slbv;
}

int pmap_vsid = 1;

struct slb_desc *
pmap_slbd_alloc(pmap_t pm, vaddr_t va)
{
	uint64_t esid = va >> ADDR_ESID_SHIFT;
	struct slb_desc *slbd;

	KASSERT(pm != pmap_kernel());

	slbd = pool_get(&pmap_slbd_pool, PR_NOWAIT | PR_ZERO);
	if (slbd == NULL)
		return NULL;

	slbd->slbd_esid = esid;
	slbd->slbd_vsid = pmap_vsid++;
	LIST_INSERT_HEAD(&pm->pm_slbd, slbd, slbd_list);

	/* We're almost certainly going to use it soon. */
	pmap_slbd_cache(pm, slbd);

	return slbd;
}

int
pmap_set_user_slb(pmap_t pm, vaddr_t va)
{
	struct cpu_info *ci = curcpu();
	struct slb_desc *slbd;
	uint64_t slbe, slbv;

	KASSERT(pm != pmap_kernel());

	slbd = pmap_slbd_lookup(pm, va);
	if (slbd == NULL) {
		slbd = pmap_slbd_alloc(pm, va);
		if (slbd == NULL)
			return EFAULT;
	}

	slbe = (slbd->slbd_esid << SLBE_ESID_SHIFT) | SLBE_VALID | 31;
	slbv = slbd->slbd_vsid << SLBV_VSID_SHIFT;

	ci->ci_kernel_slb[31].slb_slbe = slbe;
	ci->ci_kernel_slb[31].slb_slbv = slbv;

	isync();
	slbmte(slbv, slbe);
	isync();

	return 0;
}

void
pmap_unset_user_slb(void)
{
	struct cpu_info *ci = curcpu();

	isync();
	slbie(ci->ci_kernel_slb[31].slb_slbe);
	isync();
	
	ci->ci_kernel_slb[31].slb_slbe = 0;
	ci->ci_kernel_slb[31].slb_slbv = 0;
}

/*
 * VP routines, virtual to physical translation information.
 * These data structures are based off of the pmap, per process.
 */

struct pte_desc *
pmap_vp_lookup(pmap_t pm, vaddr_t va)
{
	struct slb_desc *slbd;
	struct pmapvp1 *vp1;
	struct pmapvp2 *vp2;

	slbd = pmap_slbd_lookup(pm, va);
	if (slbd == NULL)
		return NULL;

	vp1 = slbd->slbd_vp;
	if (vp1 == NULL)
		return NULL;

	vp2 = vp1->vp[VP_IDX1(va)];
	if (vp2 == NULL)
		return NULL;

	return vp2->vp[VP_IDX2(va)];
}

/*
 * Remove, and return, pted at specified address, NULL if not present.
 */
struct pte_desc *
pmap_vp_remove(pmap_t pm, vaddr_t va)
{
	struct slb_desc *slbd;
	struct pmapvp1 *vp1;
	struct pmapvp2 *vp2;
	struct pte_desc *pted;

	slbd = pmap_slbd_lookup(pm, va);
	if (slbd == NULL)
		return NULL;

	vp1 = slbd->slbd_vp;
	if (vp1 == NULL)
		return NULL;

	vp2 = vp1->vp[VP_IDX1(va)];
	if (vp2 == NULL)
		return NULL;

	pted = vp2->vp[VP_IDX2(va)];
	vp2->vp[VP_IDX2(va)] = NULL;

	return pted;
}

/*
 * Create a V -> P mapping for the given pmap and virtual address
 * with reference to the pte descriptor that is used to map the page.
 * This code should track allocations of vp table allocations
 * so they can be freed efficiently.
 */
int
pmap_vp_enter(pmap_t pm, vaddr_t va, struct pte_desc *pted, int flags)
{
	struct slb_desc *slbd;
	struct pmapvp1 *vp1;
	struct pmapvp2 *vp2;

	slbd = pmap_slbd_lookup(pm, va);
	if (slbd == NULL) {
		slbd = pmap_slbd_alloc(pm, va);
		KASSERT(slbd);
	}

	vp1 = slbd->slbd_vp;
	if (vp1 == NULL) {
		vp1 = pool_get(&pmap_vp_pool, PR_NOWAIT | PR_ZERO);
		if (vp1 == NULL) {
			if ((flags & PMAP_CANFAIL) == 0)
				panic("%s: unable to allocate L1", __func__);
			return ENOMEM;
		}
		slbd->slbd_vp = vp1;
	}

	vp2 = vp1->vp[VP_IDX1(va)];
	if (vp2 == NULL) {
		vp2 = pool_get(&pmap_vp_pool, PR_NOWAIT | PR_ZERO);
		if (vp2 == NULL) {
			if ((flags & PMAP_CANFAIL) == 0)
				panic("%s: unable to allocate L2", __func__);
			return ENOMEM;
		}
		vp1->vp[VP_IDX1(va)] = vp2;
	}

	vp2->vp[VP_IDX2(va)] = pted;
	return 0;
}

void
pmap_enter_pv(struct pte_desc *pted, struct vm_page *pg)
{
	mtx_enter(&pg->mdpage.pv_mtx);
	LIST_INSERT_HEAD(&(pg->mdpage.pv_list), pted, pted_pv_list);
	pted->pted_va |= PTED_VA_MANAGED_M;
	mtx_leave(&pg->mdpage.pv_mtx);
}

void
pmap_remove_pv(struct pte_desc *pted)
{
	struct vm_page *pg = PHYS_TO_VM_PAGE(pted->pted_pte.pte_lo & PTE_RPGN);

	mtx_enter(&pg->mdpage.pv_mtx);
	LIST_REMOVE(pted, pted_pv_list);
	mtx_leave(&pg->mdpage.pv_mtx);
}

struct pte *
pte_lookup(uint64_t vsid, vaddr_t va)
{
	uint64_t hash, avpn, pte_hi;
	struct pte *pte;
	int idx, i;

	/* Primary hash. */
	hash = (vsid & VSID_HASH_MASK) ^ ((va & ADDR_PIDX) >> ADDR_PIDX_SHIFT);
	idx = (hash & pmap_ptab_mask);
	pte = pmap_ptable + (idx * 8);
	avpn = (vsid << PTE_VSID_SHIFT) |
	    (va & ADDR_PIDX) >> (ADDR_VSID_SHIFT - PTE_VSID_SHIFT);
	pte_hi = (avpn & PTE_AVPN) | PTE_VALID;

	for (i = 0; i < 8; i++) {
		if (pte[i].pte_hi == pte_hi)
			return &pte[i];
	}

	/* Secondary hash. */
	idx ^= pmap_ptab_mask;
	pte_hi |= PTE_HID;

	for (i = 0; i < 8; i++) {
		if (pte[i].pte_hi == pte_hi)
			return &pte[i];
	}

	return NULL;
}

/*
 * Delete a Page Table Entry, section 5.10.1.3.
 *
 * Note: hash table must be locked.
 */
void
pte_del(struct pte *pte, uint64_t ava)
{
	pte->pte_hi &= ~PTE_VALID;
	ptesync();	/* Ensure update completed. */
	tlbie(ava);	/* Invalidate old translation. */
	eieio();	/* Order tlbie before tlbsync. */
	tlbsync();	/* Ensure tlbie completed on all processors. */
	ptesync();	/* Ensure tlbsync and update completed. */
}

void
pte_zap(struct pte *pte, struct pte_desc *pted)
{
	pte_del(pte, pmap_pted2ava(pted));

	if (!PTED_MANAGED(pted))
		return;

	pmap_attr_save(pted->pted_pte.pte_lo & PTE_RPGN,
	    pte->pte_lo & (PTE_REF|PTE_CHG));
}

void
pmap_fill_pte(pmap_t pm, vaddr_t va, paddr_t pa, struct pte_desc *pted,
    vm_prot_t prot, int cache)
{
	struct pte *pte = &pted->pted_pte;

	pted->pted_pmap = pm;
	pted->pted_va = va & ~PAGE_MASK;
	pted->pted_vsid = pmap_va2vsid(pm, va);
	KASSERT(pted->pted_vsid != 0);

	pte->pte_hi = (pmap_pted2avpn(pted) & PTE_AVPN) | PTE_VALID;
	pte->pte_lo = (pa & PTE_RPGN);

	if (prot & PROT_WRITE)
		pte->pte_lo |= PTE_RW;
	else
		pte->pte_lo |= PTE_RO;
	if (prot & PROT_EXEC)
		pted->pted_va |= PTED_VA_EXEC_M;
	else
		pte->pte_lo |= PTE_N;

	if (cache == PMAP_CACHE_WB)
		pte->pte_lo |= PTE_M;
	else
		pte->pte_lo |= (PTE_M | PTE_I | PTE_G);
}

int
pmap_test_attrs(struct vm_page *pg, u_int flagbit)
{
	struct pte_desc *pted;
	uint64_t ptebit = pmap_flags2pte(flagbit);
	u_int bits = pg->pg_flags & flagbit;
	int s;

	if (bits == flagbit)
		return bits;

	mtx_enter(&pg->mdpage.pv_mtx);
	LIST_FOREACH(pted, &(pg->mdpage.pv_list), pted_pv_list) {
		struct pte *pte;

		PMAP_HASH_LOCK(s);
		if ((pte = pmap_ptedinhash(pted)) != NULL)
			bits |=	pmap_pte2flags(pte->pte_lo & ptebit);
		PMAP_HASH_UNLOCK(s);

		if (bits == flagbit)
			break;
	}
	mtx_leave(&pg->mdpage.pv_mtx);

	atomic_setbits_int(&pg->pg_flags,  bits);

	return bits;
}

int
pmap_clear_attrs(struct vm_page *pg, u_int flagbit)
{
	struct pte_desc *pted;
	uint64_t ptebit = pmap_flags2pte(flagbit);
	u_int bits = pg->pg_flags & flagbit;
	int s;

	mtx_enter(&pg->mdpage.pv_mtx);
	LIST_FOREACH(pted, &(pg->mdpage.pv_list), pted_pv_list) {
		struct pte *pte;

		PMAP_HASH_LOCK(s);
		if ((pte = pmap_ptedinhash(pted)) != NULL) {
			bits |=	pmap_pte2flags(pte->pte_lo & ptebit);

			pte_del(pte, pmap_pted2ava(pted));

			pte->pte_lo &= ~ptebit;
			eieio();
			pte->pte_hi |= PTE_VALID;
			ptesync();
		}
		PMAP_HASH_UNLOCK(s);
	}
	mtx_leave(&pg->mdpage.pv_mtx);

	/*
	 * this is done a second time, because while walking the list
	 * a bit could have been promoted via pmap_attr_save()
	 */
	bits |= pg->pg_flags & flagbit;
	atomic_clearbits_int(&pg->pg_flags,  flagbit);

	return bits;
}

void
pte_insert(struct pte_desc *pted)
{
	struct pte *pte;
	vaddr_t va;
	uint64_t vsid, hash;
	int off, idx, i;
	int s;

	PMAP_HASH_LOCK(s);

	if ((pte = pmap_ptedinhash(pted)) != NULL)
		pte_zap(pte, pted);

	pted->pted_va &= ~(PTED_VA_HID_M|PTED_VA_PTEGIDX_M);

	va = pted->pted_va & ~PAGE_MASK;
	vsid = pted->pted_vsid;
	hash = (vsid & VSID_HASH_MASK) ^ ((va & ADDR_PIDX) >> ADDR_PIDX_SHIFT);
	idx = (hash & pmap_ptab_mask);

	/*
	 * instead of starting at the beginning of each pteg,
	 * the code should pick a random location with in the primary
	 * then search all of the entries, then if not yet found,
	 * do the same for the secondary.
	 * this would reduce the frontloading of the pteg.
	 */

	/* first just try fill of primary hash */
	pte = pmap_ptable + (idx * 8);
	for (i = 0; i < 8; i++) {
		if (pte[i].pte_hi & PTE_VALID)
			continue;

		pted->pted_va |= i;

		/* Add a Page Table Entry, section 5.10.1.1. */
		pte[i].pte_hi = pted->pted_pte.pte_hi & ~PTE_VALID;
		pte[i].pte_lo = pted->pted_pte.pte_lo;
		eieio();	/* Order 1st PTE update before 2nd. */
		pte[i].pte_hi |= PTE_VALID;
		ptesync();	/* Ensure updates completed. */

		if (i > 6)
			printf("%s: primary %d\n", __func__, i);
		goto out;
	}

	/* try fill of secondary hash */
	pte = pmap_ptable + (idx ^ pmap_ptab_mask) * 8;
	for (i = 0; i < 8; i++) {
		if (pte[i].pte_hi & PTE_VALID)
			continue;

		pted->pted_va |= (i | PTED_VA_HID_M);

		/* Add a Page Table Entry, section 5.10.1.1. */
		pte[i].pte_hi = pted->pted_pte.pte_hi & ~PTE_VALID;
		pte[i].pte_lo = pted->pted_pte.pte_lo;
		eieio();	/* Order 1st PTE update before 2nd. */
		pte[i].pte_hi |= (PTE_HID|PTE_VALID);
		ptesync();	/* Ensure updates completed. */

		printf("%s: secondary %d\n", __func__, i);
		goto out;
	}

	printf("%s: replacing!\n", __func__);

	/* need decent replacement algorithm */
	off = mftb();
	pted->pted_va |= off & (PTED_VA_PTEGIDX_M|PTED_VA_HID_M);

	idx ^= (PTED_HID(pted) ? pmap_ptab_mask : 0);
	pte = pmap_ptable + (idx * 8);
	pte += PTED_PTEGIDX(pted); /* increment by index into pteg */

	if (pte->pte_hi & PTE_VALID) {
		uint64_t avpn, vpn;

		avpn = pte->pte_hi & PTE_AVPN;
		vsid = avpn >> PTE_VSID_SHIFT;
		vpn = avpn << (ADDR_VSID_SHIFT - PTE_VSID_SHIFT - PAGE_SHIFT);

		idx ^= ((pte->pte_hi & PTE_HID) ? pmap_ptab_mask : 0);
		vpn |= ((idx ^ vsid) & (ADDR_PIDX >> ADDR_PIDX_SHIFT));

		pte_del(pte, vpn << PAGE_SHIFT);

		pmap_attr_save(pte->pte_lo & PTE_RPGN,
		    pte->pte_lo & (PTE_REF|PTE_CHG));
	}

	/* Add a Page Table Entry, section 5.10.1.1. */
	pte->pte_hi = pted->pted_pte.pte_hi & ~PTE_VALID;
	if (PTED_HID(pted))
		pte->pte_hi |= PTE_HID;
	pte->pte_lo = pted->pted_pte.pte_lo;
	eieio();	/* Order 1st PTE update before 2nd. */
	pte->pte_hi |= PTE_VALID;
	ptesync();	/* Ensure updates completed. */

out:
	PMAP_HASH_UNLOCK(s);
}

void
pmap_remove_pted(pmap_t pm, struct pte_desc *pted)
{
	struct pte *pte;
	int s;

	KASSERT(pm == pted->pted_pmap);
	PMAP_VP_ASSERT_LOCKED(pm);

	pm->pm_stats.resident_count--;

	PMAP_HASH_LOCK(s);
	if ((pte = pmap_ptedinhash(pted)) != NULL)
		pte_zap(pte, pted);
	PMAP_HASH_UNLOCK(s);

	pted->pted_va &= ~PTED_VA_EXEC_M;
	pted->pted_pte.pte_hi &= ~PTE_VALID;

	if (PTED_MANAGED(pted))
		pmap_remove_pv(pted);

	pmap_vp_remove(pm, pted->pted_va);
	pool_put(&pmap_pted_pool, pted);
}

extern struct fdt_reg memreg[];
extern int nmemreg;

#ifdef DDB
extern struct fdt_reg initrd_reg;
#endif

void memreg_add(const struct fdt_reg *);
void memreg_remove(const struct fdt_reg *);

vaddr_t vmmap;
vaddr_t zero_page;
vaddr_t copy_src_page;
vaddr_t copy_dst_page;
vaddr_t virtual_avail = VM_MIN_KERNEL_ADDRESS;

void *
pmap_steal_avail(size_t size, size_t align)
{
	struct fdt_reg reg;
	uint64_t start, end;
	int i;

	for (i = 0; i < nmemreg; i++) {
		if (memreg[i].size > size) {
			start = (memreg[i].addr + (align - 1)) & ~(align - 1);
			end = start + size;
			if (end <= memreg[i].addr + memreg[i].size) {
				reg.addr = start;
				reg.size = end - start;
				memreg_remove(&reg);
				return (void *)start;
			}
		}
	}
	panic("can't allocate");
}

void
pmap_virtual_space(vaddr_t *start, vaddr_t *end)
{
	*start = virtual_avail;
	*end = VM_MAX_KERNEL_ADDRESS;
}

pmap_t
pmap_create(void)
{
	pmap_t pm;

	pm = pool_get(&pmap_pmap_pool, PR_WAITOK | PR_ZERO);
	pm->pm_refs = 1;
	LIST_INIT(&pm->pm_slbd);
	return pm;
}

/*
 * Add a reference to a given pmap.
 */
void
pmap_reference(pmap_t pm)
{
	atomic_inc_int(&pm->pm_refs);
}

/*
 * Retire the given pmap from service.
 * Should only be called if the map contains no valid mappings.
 */
void
pmap_destroy(pmap_t pm)
{
	int refs;

	refs = atomic_dec_int_nv(&pm->pm_refs);
	if (refs > 0)
		return;

	/*
	 * reference count is zero, free pmap resources and free pmap.
	 */
	pmap_release(pm);
	pool_put(&pmap_pmap_pool, pm);
}

/*
 * Release any resources held by the given physical map.
 * Called when a pmap initialized by pmap_pinit is being released.
 */
void
pmap_release(pmap_t pm)
{
	pmap_vp_destroy(pm);
}

void
pmap_vp_destroy(pmap_t pm)
{
	struct slb_desc *slbd;
	struct pmapvp1 *vp1;
	struct pmapvp2 *vp2;
	struct pte_desc *pted;
	int i, j;

	LIST_FOREACH(slbd, &pm->pm_slbd, slbd_list) {
		vp1 = slbd->slbd_vp;
		if (vp1 == NULL)
			continue;

		for (i = 0; i < VP_IDX1_CNT; i++) {
			vp2 = vp1->vp[i];
			if (vp2 == NULL)
				continue;
			vp1->vp[i] = NULL;

			for (j = 0; j < VP_IDX2_CNT; j++) {
				pted = vp2->vp[j];
				if (pted == NULL)
					continue;
				vp2->vp[j] = NULL;

				pool_put(&pmap_pted_pool, pted);
			}
			pool_put(&pmap_vp_pool, vp2);
		}
		slbd->slbd_vp = NULL;
		pool_put(&pmap_vp_pool, vp1);
	}

	/* XXX Free SLB descriptors. */
}

void
pmap_init(void)
{
	int i;

	pool_init(&pmap_pmap_pool, sizeof(struct pmap), 0, IPL_NONE, 0,
	    "pmap", &pool_allocator_single);
	pool_setlowat(&pmap_pmap_pool, 2);
	pool_init(&pmap_vp_pool, sizeof(struct pmapvp1), 0, IPL_VM, 0,
	    "vp", &pool_allocator_single);
	pool_setlowat(&pmap_vp_pool, 10);
	pool_init(&pmap_pted_pool, sizeof(struct pte_desc), 0, IPL_VM, 0,
	    "pted", NULL);
	pool_setlowat(&pmap_pted_pool, 20);
	pool_init(&pmap_slbd_pool, sizeof(struct slb_desc), 0, IPL_VM, 0,
	    "slbd", NULL);
	pool_setlowat(&pmap_slbd_pool, 5);

	PMAP_HASH_LOCK_INIT();

	LIST_INIT(&pmap_kernel()->pm_slbd);
	for (i = 0; i < nitems(kernel_slb_desc); i++) {
		LIST_INSERT_HEAD(&pmap_kernel()->pm_slbd,
		    &kernel_slb_desc[i], slbd_list);
	}

	pmap_initialized = 1;
}

void
pmap_copy(pmap_t dst_pmap, pmap_t src_pmap, vaddr_t dst_addr,
    vsize_t len, vaddr_t src_addr)
{
}

int
pmap_enter(pmap_t pm, vaddr_t va, paddr_t pa, vm_prot_t prot, int flags)
{
	struct pte_desc *pted;
	struct vm_page *pg;
	int cache = PMAP_CACHE_WB;
	int need_sync = 0;
	int error = 0;

	if (pa & PMAP_NOCACHE)
		cache = PMAP_CACHE_CI;
	pg = PHYS_TO_VM_PAGE(pa);
	if (!pmap_initialized)
		printf("%s\n", __func__);

	PMAP_VP_LOCK(pm);
	pted = pmap_vp_lookup(pm, va);
	if (pted && PTED_VALID(pted)) {
		pmap_remove_pted(pm, pted);
		pted = NULL;
	}

	pm->pm_stats.resident_count++;

	/* Do not have pted for this, get one and put it in VP */
	if (pted == NULL) {
		pted = pool_get(&pmap_pted_pool, PR_NOWAIT | PR_ZERO);
		if (pted == NULL) {
			if ((flags & PMAP_CANFAIL) == 0)
				panic("%s: failed to allocate pted", __func__);
			error = ENOMEM;
			goto out;
		}
		if (pmap_vp_enter(pm, va, pted, flags)) {
			if ((flags & PMAP_CANFAIL) == 0)
				panic("%s: failed to allocate L2/L3", __func__);
			error = ENOMEM;
			pool_put(&pmap_pted_pool, pted);
			goto out;
		}
	}

	pmap_fill_pte(pm, va, pa, pted, prot, cache);

	if (pg != NULL)
		pmap_enter_pv(pted, pg); /* only managed mem */

	pte_insert(pted);

	if (prot & PROT_EXEC) {
		if (pg != NULL) {
			need_sync = ((pg->pg_flags & PG_PMAP_EXE) == 0);
			if (prot & PROT_WRITE)
				atomic_clearbits_int(&pg->pg_flags,
				    PG_PMAP_EXE);
			else
				atomic_setbits_int(&pg->pg_flags,
				    PG_PMAP_EXE);
		} else
			need_sync = 1;
	} else {
		/*
		 * Should we be paranoid about writeable non-exec 
		 * mappings ? if so, clear the exec tag
		 */
		if ((prot & PROT_WRITE) && (pg != NULL))
			atomic_clearbits_int(&pg->pg_flags, PG_PMAP_EXE);
	}

	if (need_sync)
		pmap_pted_syncicache(pted);

out:
	PMAP_VP_UNLOCK(pm);
	return error;
}

void
pmap_remove(pmap_t pm, vaddr_t sva, vaddr_t eva)
{
	struct pte_desc *pted;
	vaddr_t va;

	PMAP_VP_LOCK(pm);
	for (va = sva; va < eva; va += PAGE_SIZE) {
		pted = pmap_vp_lookup(pm, va);
		if (pted && PTED_VALID(pted))
			pmap_remove_pted(pm, pted);
	}
	PMAP_VP_UNLOCK(pm);
}

void
pmap_pted_syncicache(struct pte_desc *pted)
{
	paddr_t pa = pted->pted_pte.pte_lo & PTE_RPGN;
	vaddr_t va = pted->pted_va & ~PAGE_MASK;

	if (pted->pted_pmap != pmap_kernel()) {
		pmap_kenter_pa(zero_page, pa, PROT_READ | PROT_WRITE);
		va = zero_page;
	}

	__syncicache((void *)va, PAGE_SIZE);

	if (pted->pted_pmap != pmap_kernel())
		pmap_kremove(zero_page, PAGE_SIZE);
}

void
pmap_pted_ro(struct pte_desc *pted, vm_prot_t prot)
{
	struct vm_page *pg;
	struct pte *pte;
	int s;

	pg = PHYS_TO_VM_PAGE(pted->pted_pte.pte_lo & PTE_RPGN);
	if (pg->pg_flags & PG_PMAP_EXE) {
		if ((prot & (PROT_WRITE | PROT_EXEC)) == PROT_WRITE)
			atomic_clearbits_int(&pg->pg_flags, PG_PMAP_EXE);
		else
			pmap_pted_syncicache(pted);
	}

	pted->pted_pte.pte_lo &= ~PTE_PP;
	pted->pted_pte.pte_lo |= PTE_RO;

	if ((prot & PROT_EXEC) == 0)
		pted->pted_pte.pte_lo |= PTE_N;

	PMAP_HASH_LOCK(s);
	if ((pte = pmap_ptedinhash(pted)) != NULL) {
		pte_del(pte, pmap_pted2ava(pted));

		/* XXX Use pte_zap instead? */
		if (PTED_MANAGED(pted)) {
			pmap_attr_save(pte->pte_lo & PTE_RPGN,
			    pte->pte_lo & (PTE_REF|PTE_CHG));
		}

		/* Add a Page Table Entry, section 5.10.1.1. */
		pte->pte_lo &= ~(PTE_CHG|PTE_PP);
		pte->pte_lo |= PTE_RO;
		eieio();	/* Order 1st PTE update before 2nd. */
		pte->pte_hi |= PTE_VALID;
		ptesync();	/* Ensure updates completed. */
	}
	PMAP_HASH_UNLOCK(s);
}

/*
 * Lower the protection on the specified physical page.
 *
 * There are only two cases, either the protection is going to 0,
 * or it is going to read-only.
 */
void
pmap_page_protect(struct vm_page *pg, vm_prot_t prot)
{
	struct pte_desc *pted;
	void *pte;
	pmap_t pm;
	int s;

	if (prot == PROT_NONE) {
		mtx_enter(&pg->mdpage.pv_mtx);
		while ((pted = LIST_FIRST(&(pg->mdpage.pv_list))) != NULL) {
			pmap_reference(pted->pted_pmap);
			pm = pted->pted_pmap;
			mtx_leave(&pg->mdpage.pv_mtx);

			PMAP_VP_LOCK(pm);

			/*
			 * We dropped the pvlist lock before grabbing
			 * the pmap lock to avoid lock ordering
			 * problems.  This means we have to check the
			 * pvlist again since somebody else might have
			 * modified it.  All we care about is that the
			 * pvlist entry matches the pmap we just
			 * locked.  If it doesn't, unlock the pmap and
			 * try again.
			 */
			mtx_enter(&pg->mdpage.pv_mtx);
			if ((pted = LIST_FIRST(&(pg->mdpage.pv_list))) == NULL ||
			    pted->pted_pmap != pm) {
				mtx_leave(&pg->mdpage.pv_mtx);
				PMAP_VP_UNLOCK(pm);
				pmap_destroy(pm);
				mtx_enter(&pg->mdpage.pv_mtx);
				continue;
			}

			PMAP_HASH_LOCK(s);
			if ((pte = pmap_ptedinhash(pted)) != NULL)
				pte_zap(pte, pted);
			PMAP_HASH_UNLOCK(s);

			pted->pted_va &= ~PTED_VA_MANAGED_M;
			LIST_REMOVE(pted, pted_pv_list);
			mtx_leave(&pg->mdpage.pv_mtx);

			pmap_remove_pted(pm, pted);

			PMAP_VP_UNLOCK(pm);
			pmap_destroy(pm);
			mtx_enter(&pg->mdpage.pv_mtx);
		}
		mtx_leave(&pg->mdpage.pv_mtx);
		/* page is being reclaimed, sync icache next use */
		atomic_clearbits_int(&pg->pg_flags, PG_PMAP_EXE);
		return;
	}

	mtx_enter(&pg->mdpage.pv_mtx);
	LIST_FOREACH(pted, &(pg->mdpage.pv_list), pted_pv_list)
		pmap_pted_ro(pted, prot);
	mtx_leave(&pg->mdpage.pv_mtx);
}

void
pmap_protect(pmap_t pm, vaddr_t sva, vaddr_t eva, vm_prot_t prot)
{
	if (prot & (PROT_READ | PROT_EXEC)) {
		struct pte_desc *pted;

		PMAP_VP_LOCK(pm);
		while (sva < eva) {
			pted = pmap_vp_lookup(pm, sva);
			if (pted && PTED_VALID(pted))
				pmap_pted_ro(pted, prot);
			sva += PAGE_SIZE;
		}
		PMAP_VP_UNLOCK(pm);
		return;
	}
	pmap_remove(pm, sva, eva);
}

void
pmap_kenter_pa(vaddr_t va, paddr_t pa, vm_prot_t prot)
{
	pmap_t pm = pmap_kernel();
	struct pte_desc pted;
	struct vm_page *pg;
	int cache = (pa & PMAP_NOCACHE) ? PMAP_CACHE_CI : PMAP_CACHE_WB;

	pm->pm_stats.resident_count++;

	if (prot & PROT_WRITE) {
		pg = PHYS_TO_VM_PAGE(pa);
		if (pg != NULL)
			atomic_clearbits_int(&pg->pg_flags, PG_PMAP_EXE);
	}

	/* Calculate PTE */
	pmap_fill_pte(pm, va, pa, &pted, prot, cache);
	pted.pted_va |= PTED_VA_WIRED_M;

	/* Insert into HTAB */
	pte_insert(&pted);
}

void
pmap_kremove(vaddr_t va, vsize_t len)
{
	pmap_t pm = pmap_kernel();
	vaddr_t eva = va + len;
	struct pte *pte;
	uint64_t vsid;
	int s;

	while (va < eva) {
		vsid = pmap_kernel_vsid(va >> ADDR_ESID_SHIFT);

		PMAP_HASH_LOCK(s);
		pte = pte_lookup(vsid, va);
		if (pte)
			pte_del(pte, pmap_ava(vsid, va));
		PMAP_HASH_UNLOCK(s);

		if (pte)
			pm->pm_stats.resident_count--;

		va += PAGE_SIZE;
	}
}

int
pmap_is_referenced(struct vm_page *pg)
{
	return pmap_test_attrs(pg, PG_PMAP_REF);
}

int
pmap_is_modified(struct vm_page *pg)
{
	return pmap_test_attrs(pg, PG_PMAP_MOD);
}

int
pmap_clear_reference(struct vm_page *pg)
{
	return pmap_clear_attrs(pg, PG_PMAP_REF);
}

int
pmap_clear_modify(struct vm_page *pg)
{
	return pmap_clear_attrs(pg, PG_PMAP_MOD);
}

int
pmap_extract(pmap_t pm, vaddr_t va, paddr_t *pa)
{
	struct pte *pte;
	uint64_t vsid;
	int s;

	if (pm == pmap_kernel() &&
	    va >= (vaddr_t)_start && va < (vaddr_t)_end) {
		*pa = va;
		return 1;
	}

	vsid = pmap_va2vsid(pm, va);
	if (vsid == 0)
		return 0;

	PMAP_HASH_LOCK(s);
	pte = pte_lookup(vsid, va);
	if (pte)
		*pa = (pte->pte_lo & PTE_RPGN) | (va & PAGE_MASK);
	PMAP_HASH_UNLOCK(s);

	return (pte != NULL);
}

void
pmap_activate(struct proc *p)
{
}

void
pmap_deactivate(struct proc *p)
{
}

void
pmap_unwire(pmap_t pm, vaddr_t va)
{
}

void
pmap_collect(pmap_t pm)
{
}

void
pmap_zero_page(struct vm_page *pg)
{
	paddr_t pa = VM_PAGE_TO_PHYS(pg);
	paddr_t va = zero_page + cpu_number() * PAGE_SIZE;

	pmap_kenter_pa(va, pa, PROT_READ | PROT_WRITE);
	memset((void *)va, 0, PAGE_SIZE);
	pmap_kremove(va, PAGE_SIZE);
}

void
pmap_copy_page(struct vm_page *srcpg, struct vm_page *dstpg)
{
	paddr_t srcpa = VM_PAGE_TO_PHYS(srcpg);
	paddr_t dstpa = VM_PAGE_TO_PHYS(dstpg);
	vaddr_t srcva = copy_src_page + cpu_number() * PAGE_SIZE;
	vaddr_t dstva = copy_dst_page + cpu_number() * PAGE_SIZE;

	pmap_kenter_pa(srcva, srcpa, PROT_READ);
	pmap_kenter_pa(dstva, dstpa, PROT_READ | PROT_WRITE);
	memcpy((void *)dstva, (void *)srcva, PAGE_SIZE);
	pmap_kremove(srcva, PAGE_SIZE);
	pmap_kremove(dstva, PAGE_SIZE);
}

void
pmap_proc_iflush(struct process *pr, vaddr_t va, vsize_t len)
{
	panic(__func__);
}

void
pmap_set_kernel_slb(int idx, vaddr_t va)
{
	struct cpu_info *ci = curcpu();
	uint64_t esid, slbe, slbv;

	esid = va >> ADDR_ESID_SHIFT;
	kernel_slb_desc[idx].slbd_esid = esid;
	slbe = (esid << SLBE_ESID_SHIFT) | SLBE_VALID | idx;
	slbv = pmap_kernel_vsid(esid) << SLBV_VSID_SHIFT;
	slbmte(slbv, slbe);

	ci->ci_kernel_slb[idx].slb_slbe = slbe;
	ci->ci_kernel_slb[idx].slb_slbv = slbv;
}

void
pmap_bootstrap(void)
{
	paddr_t start, end, pa;
	vaddr_t va;
	vm_prot_t prot;
	int idx = 0;

	/* Clear SLB. */
	slbia();
	slbie(slbmfee(0));

	/* Clear TLB. */
	tlbia();

#define HTABENTS 2048

	pmap_ptab_cnt = HTABENTS;
	while (pmap_ptab_cnt * 2 < physmem)
		pmap_ptab_cnt <<= 1;

	/*
	 * allocate suitably aligned memory for HTAB
	 */
	pmap_ptable = pmap_steal_avail(HTABMEMSZ, HTABMEMSZ);
	memset(pmap_ptable, 0, HTABMEMSZ);
	pmap_ptab_mask = pmap_ptab_cnt - 1;

	/* Map page tables. */
	start = (paddr_t)pmap_ptable;
	end = start + HTABMEMSZ;
	for (pa = start; pa < end; pa += PAGE_SIZE)
		pmap_kenter_pa(pa, pa, PROT_READ | PROT_WRITE);

	/* Map kernel. */
	start = (paddr_t)_start;
	end = (paddr_t)_end;
	for (pa = start; pa < end; pa += PAGE_SIZE) {
		if (pa < (paddr_t)_etext)
			prot = PROT_READ | PROT_EXEC;
		else
			prot = PROT_READ | PROT_WRITE;
		pmap_kenter_pa(pa, pa, prot);
	}

#ifdef DDB
	/* Map initrd. */
	start = initrd_reg.addr;
	end = initrd_reg.addr + initrd_reg.size;
	for (pa = start; pa < end; pa += PAGE_SIZE)
		pmap_kenter_pa(pa, pa, PROT_READ | PROT_WRITE);
#endif

	/* Allocate partition table. */
	pmap_pat = pmap_steal_avail(PATMEMSZ, PATMEMSZ);
	memset(pmap_pat, 0, PATMEMSZ);
	pmap_pat[0].pate_htab = (paddr_t)pmap_ptable | HTABSIZE;
	mtptcr((paddr_t)pmap_pat | PATSIZE);

	/* SLB entry for the kernel. */
	pmap_set_kernel_slb(idx++, (vaddr_t)_start);

	/* SLB entry for the page tables. */
	pmap_set_kernel_slb(idx++, (vaddr_t)pmap_ptable);

	/* SLB entries for kernel VA. */
	for (va = VM_MIN_KERNEL_ADDRESS; va < VM_MAX_KERNEL_ADDRESS;
	     va += 256 * 1024 * 1024)
		pmap_set_kernel_slb(idx++, va);

	vmmap = virtual_avail;
	virtual_avail += PAGE_SIZE;
	zero_page = virtual_avail;
	virtual_avail += MAXCPUS * PAGE_SIZE;
	copy_src_page = virtual_avail;
	virtual_avail += MAXCPUS * PAGE_SIZE;
	copy_dst_page = virtual_avail;
	virtual_avail += MAXCPUS * PAGE_SIZE;
}

#ifdef DDB
/*
 * DDB will edit the PTE to gain temporary write access to a page in
 * the read-only kernel text.
 */
struct pte *
pmap_get_kernel_pte(vaddr_t va)
{
	uint64_t vsid;

	vsid = pmap_kernel_vsid(va >> ADDR_ESID_SHIFT);
	return pte_lookup(vsid, va);
}
#endif
