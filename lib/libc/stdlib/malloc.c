/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: malloc.c,v 1.18 1996/11/23 19:10:26 niklas Exp $";
#endif /* LIBC_SCCS and not lint */

/*
 * Defining EXTRA_SANITY will enable extra checks which are related
 * to internal conditions and consistency in malloc.c. This has a
 * noticeable runtime performance hit, and generally will not do you
 * any good unless you fiddle with the internals of malloc or want
 * to catch random pointer corruption as early as possible.
 */
#undef EXTRA_SANITY

/*
 * Defining MALLOC_STATS will enable you to call malloc_dump() and set
 * the [dD] options in the MALLOC_OPTIONS environment variable.
 * It has no run-time performance hit, but does pull in stdio...
 */
#undef MALLOC_STATS

#if defined(EXTRA_SANITY) && !defined(MALLOC_STATS)
# define MALLOC_STATS	/* required for EXTRA_SANITY */
#endif

/*
 * What to use for Junk.  This is the byte value we use to fill with
 * when the 'J' option is enabled.
 */
#define SOME_JUNK	0xd0		/* as in "Duh" :-) */

/*
 * No user serviceable parts behind this point.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/mman.h>

/*
 * If these weren't defined here, they would be calculated on the fly,
 * at a considerable cost in performance.
 */
#ifdef __OpenBSD__
#   if defined(__alpha__) || defined(__m68k__) || defined(__mips__) || \
       defined(__i386__) || defined(__m88k__) || defined(__ns32k__) || \
       defined(__vax__)
#	define	malloc_pagesize		(NBPG)
#	define	malloc_pageshift	(PGSHIFT)
#	define	malloc_maxsize		(malloc_pagesize >> 1)
#	define	malloc_minsize		16U
#   endif /* __i386__ */
#endif /* __OpenBSD__ */

/*
 * This structure describes a page worth of chunks.
 */

struct pginfo {
    struct pginfo	*next;	/* next on the free list */
    void		*page;	/* Pointer to the page */
    u_short		size;	/* size of this page's chunks */
    u_short		shift;	/* How far to shift for this size chunks */
    u_short		free;	/* How many free chunks */
    u_short		total;	/* How many chunk */
    u_long		bits[1]; /* Which chunks are free */
};

/*
 * This structure describes a number of free pages.
 */

struct pgfree {
    struct pgfree	*next;	/* next run of free pages */
    struct pgfree	*prev;	/* prev run of free pages */
    void		*page;	/* pointer to free pages */
    void		*end;	/* pointer to end of free pages */
    u_long		size;	/* number of bytes free */
};

/*
 * How many bits per u_long in the bitmap.
 * Change only if not 8 bits/byte
 */
#define	MALLOC_BITS	(8*sizeof(u_long))

/*
 * Magic values to put in the page_directory
 */
#define MALLOC_NOT_MINE	((struct pginfo*) 0)
#define MALLOC_FREE 	((struct pginfo*) 1)
#define MALLOC_FIRST	((struct pginfo*) 2)
#define MALLOC_FOLLOW	((struct pginfo*) 3)
#define MALLOC_MAGIC	((struct pginfo*) 4)

/*
 * The i386 architecture has some very convenient instructions.
 * We might as well use them.  There are C-language backups, but
 * they are considerably slower.
 */
#if defined(__i386__) && defined(__GNUC__)
#define ffs _ffs
static __inline int
_ffs(input)
	unsigned input;
{
	int result;
	__asm("bsfl %1, %0" : "=r" (result) : "r" (input));
	return result+1;
}

#define fls _fls
static __inline int
_fls(input)
	unsigned input;
{
	int result;
	__asm("bsrl %1, %0" : "=r" (result) : "r" (input));
	return result+1;
}

#define set_bit _set_bit
static __inline void
_set_bit(pi, bit)
	struct pginfo *pi;
	int bit;
{
	__asm("btsl %0, (%1)" :
	: "r" (bit & (MALLOC_BITS-1)), "r" (pi->bits+(bit/MALLOC_BITS)));
}

#define clr_bit _clr_bit
static __inline void
_clr_bit(pi, bit)
	struct pginfo *pi;
	int bit;
{
	__asm("btcl %0, (%1)" :
	: "r" (bit & (MALLOC_BITS-1)), "r" (pi->bits+(bit/MALLOC_BITS)));
}

#endif /* __i386__ && __GNUC__ */

/*
 * Set to one when malloc_init has been called
 */
static	unsigned	initialized;

/*
 * The size of a page.
 * Must be a integral multiplum of the granularity of mmap(2).
 * Your toes will curl if it isn't a power of two
 */
#ifndef malloc_pagesize
static	unsigned        malloc_pagesize;
#endif /* malloc_pagesize */

/* A mask for the offset inside a page.  */
#define malloc_pagemask	((malloc_pagesize)-1)

#define pageround(foo) (((foo) + (malloc_pagemask))&(~(malloc_pagemask)))
#define ptr2index(foo) (((u_long)(foo) >> malloc_pageshift)-malloc_origo)

/* malloc_pagesize == 1 << malloc_pageshift */
#ifndef malloc_pageshift
static	unsigned	malloc_pageshift;
#endif /* malloc_pageshift */

/*
 * The smallest allocation we bother about.
 * Must be power of two
 */
#ifndef malloc_minsize
static	unsigned  malloc_minsize;
#endif /* malloc_minsize */

/*
 * The largest chunk we care about.
 * Must be smaller than pagesize
 * Must be power of two
 */
#ifndef malloc_maxsize
static	unsigned  malloc_maxsize;
#endif /* malloc_maxsize */

/* The minimum size (in pages) of the free page cache.  */
static	unsigned  malloc_cache = 16;

/* The offset from pagenumber to index into the page directory */
static	u_long  malloc_origo;

/* The last index in the page directory we care about */
static	u_long  last_index;

/* Pointer to page directory. Allocated "as if with" malloc */
static	struct	pginfo **page_dir;

/* How many slots in the page directory */
static	unsigned	malloc_ninfo;

/* Free pages line up here */
static struct pgfree	free_list;

/* Abort(), user doesn't handle problems.  */
static int malloc_abort;

/* Are we trying to die ?  */
static int suicide;

#ifdef MALLOC_STATS
/* dump statistics */
static int malloc_stats;
#endif /* MALLOC_STATS */

/* always realloc ?  */
static int malloc_realloc;

#ifdef __FreeBSD__
/* pass the kernel a hint on free pages ?  */
static int malloc_hint;
#endif

/* zero fill ?  */
static int malloc_zero;

/* junk fill ?  */
static int malloc_junk;

#ifdef __FreeBSD__
/* utrace ?  */
static int malloc_utrace;

struct ut { void *p; size_t s; void *r; };

#define UTRACE(a, b, c) \
	if (malloc_utrace) \
		{struct ut u; u.p=a; u.s = b; u.r=c; utrace(&u, sizeof u);}
#else /* !__FreeBSD__ */
#define UTRACE(a,b,c)
#endif

/* my last break. */
static void *malloc_brk;

/* one location cache for free-list holders */
static struct pgfree *px;

/* compile-time options */
char *malloc_options;

/*
 * Necessary function declarations
 */
static int extend_pgdir(u_long index);
static void *imalloc(size_t size);
static void ifree(void *ptr);
static void *irealloc(void *ptr, size_t size);

#ifdef MALLOC_STATS
void
malloc_dump(fd)
    FILE *fd;
{
    struct pginfo **pd;
    struct pgfree *pf;
    int j;

    pd = page_dir;

    /* print out all the pages */
    for(j=0;j<=last_index;j++) {
	fprintf(fd, "%08lx %5d ", (j+malloc_origo) << malloc_pageshift, j);
	if (pd[j] == MALLOC_NOT_MINE) {
	    for(j++;j<=last_index && pd[j] == MALLOC_NOT_MINE;j++)
		;
	    j--;
	    fprintf(fd, ".. %5d not mine\n",	j);
	} else if (pd[j] == MALLOC_FREE) {
	    for(j++;j<=last_index && pd[j] == MALLOC_FREE;j++)
		;
	    j--;
	    fprintf(fd, ".. %5d free\n", j);
	} else if (pd[j] == MALLOC_FIRST) {
	    for(j++;j<=last_index && pd[j] == MALLOC_FOLLOW;j++)
		;
	    j--;
	    fprintf(fd, ".. %5d in use\n", j);
	} else if (pd[j] < MALLOC_MAGIC) {
	    fprintf(fd, "(%p)\n", pd[j]);
	} else {
	    fprintf(fd, "%p %d (of %d) x %d @ %p --> %p\n",
		pd[j], pd[j]->free, pd[j]->total,
		pd[j]->size, pd[j]->page, pd[j]->next);
	}
    }

    for(pf=free_list.next; pf; pf=pf->next) {
	fprintf(fd, "Free: @%p [%p...%p[ %ld ->%p <-%p\n",
		pf, pf->page, pf->end, pf->size, pf->prev, pf->next);
	if (pf == pf->next) {
 		fprintf(fd, "Free_list loops.\n");
		break;
	}
    }

    /* print out various info */
    fprintf(fd, "Minsize\t%d\n", malloc_minsize);
    fprintf(fd, "Maxsize\t%d\n", malloc_maxsize);
    fprintf(fd, "Pagesize\t%d\n", malloc_pagesize);
    fprintf(fd, "Pageshift\t%d\n", malloc_pageshift);
    fprintf(fd, "FirstPage\t%ld\n", malloc_origo);
    fprintf(fd, "LastPage\t%ld %lx\n", last_index+malloc_pageshift,
	(last_index + malloc_pageshift) << malloc_pageshift);
    fprintf(fd, "Break\t%ld\n", (u_long)sbrk(0) >> malloc_pageshift);
}
#endif /* MALLOC_STATS */

static char *malloc_func;

static void
wrterror(p)
    char *p;
{
    char *q = "Malloc error: ";
    suicide = 1;
    write(2, q, strlen(q));
    write(2, malloc_func, strlen(malloc_func));
    write(2, p, strlen(p));
#ifdef MALLOC_STATS
    if (malloc_stats)
	malloc_dump(stderr);
#endif /* MALLOC_STATS */
    abort();
}

static void
wrtwarning(p)
    char *p;
{
    char *q = "Malloc warning: ";
    if (malloc_abort)
	wrterror(p);
    write(2, q, strlen(q));
    write(2, malloc_func, strlen(malloc_func));
    write(2, p, strlen(p));
}

#ifdef EXTRA_SANITY
static void
malloc_exit()
{
    FILE *fd = fopen("malloc.out", "a");
    char *q = "malloc() warning: Couldn't dump stats.\n";
    if (fd) {
        malloc_dump(fd);
	fclose(fd);
    } else
	write(2, q, strlen(q));
}
#endif /* EXTRA_SANITY */


/*
 * Allocate a number of pages from the OS
 */
static caddr_t
map_pages(pages)
    int pages;
{
    caddr_t result, tail;

    result = (caddr_t)pageround((u_long)sbrk(0));
    tail = result + (pages << malloc_pageshift);

    if (brk(tail)) {
#ifdef EXTRA_SANITY
	wrterror("(ES): map_pages fails\n");
#endif /* EXTRA_SANITY */
	return 0;
    }

    last_index = ptr2index(tail) - 1;
    malloc_brk = tail;

    if ((last_index+1) >= malloc_ninfo && !extend_pgdir(last_index))
	return 0;

    return result;
}

/*
 * Set a bit in the bitmap
 */
#ifndef set_bit
static __inline void
set_bit(pi, bit)
    struct pginfo *pi;
    int bit;
{
    pi->bits[bit/MALLOC_BITS] |= 1UL<<(bit%MALLOC_BITS);
}
#endif /* set_bit */

/*
 * Clear a bit in the bitmap
 */
#ifndef clr_bit
static __inline void
clr_bit(pi, bit)
    struct pginfo *pi;
    int bit;
{
    pi->bits[bit/MALLOC_BITS] &= ~(1UL<<(bit%MALLOC_BITS));
}
#endif /* clr_bit */

#ifndef tst_bit
/*
 * Test a bit in the bitmap
 */
static __inline int
tst_bit(pi, bit)
    struct pginfo *pi;
    int bit;
{
    return pi->bits[bit/MALLOC_BITS] & (1UL<<(bit%MALLOC_BITS));
}
#endif /* tst_bit */

/*
 * Find last bit
 */
#ifndef fls
static __inline int
fls(size)
    size_t size;
{
    int i = 1;
    while (size >>= 1)
	i++;
    return i;
}
#endif /* fls */

#if LONG_BIT == WORD_BIT
#define ffs_ul ffs
#else
static __inline int
ffs_ul(u_long ul)
{
    u_int n;
    int i;
    int k;

    for (i = 0; i < sizeof (u_long) / sizeof (u_int); i++) {
	n = ul & UINT_MAX;
	k = ffs (n);
	if (k)
	    break;
	ul >>= (sizeof (u_int) * 8);
    }
    if (k)
	k += i * sizeof (u_int) * 8;
    return k;
}
#endif

/*
 * Extend page directory
 */
static int
extend_pgdir(index)
    u_long index;
{
    struct  pginfo **new, **old;
    int i;
    size_t oldlen;

    /* Make it this many pages */
    i = index * sizeof *page_dir;
    i /= malloc_pagesize;
    i += 2;

    /* remember the old mapping size */
    oldlen = malloc_ninfo * sizeof *page_dir;

    /*
     * NOTE: we allocate new pages and copy the directory rather than tempt
     * fate by trying to "grow" the region.. There is nothing to prevent
     * us from accidently re-mapping space that's been allocated by our caller
     * via dlopen() or other mmap().
     *
     * The copy problem is not too bad, as there is 4K of page index per
     * 4MB of malloc arena.
     *
     * We can totally avoid the copy if we open a file descriptor to associate
     * the anon mappings with.  Then, when we remap the pages at the new
     * address, the old pages will be "magically" remapped..  But this means
     * keeping open a "secret" file descriptor.....
     */

    /* Get new pages */
    new = (struct pginfo**) mmap(0, i * malloc_pagesize, PROT_READ|PROT_WRITE,
				 MAP_ANON|MAP_PRIVATE, -1, (off_t)0);
    if (new == (struct pginfo **)-1)
	return 0;

    /* Copy the old stuff */
    memcpy(new, page_dir,
	    malloc_ninfo * sizeof *page_dir);

    /* register the new size */
    malloc_ninfo = i * malloc_pagesize / sizeof *page_dir;

    /* swap the pointers */
    old = page_dir;
    page_dir = new;

    /* Now free the old stuff */
    munmap((caddr_t)old, oldlen);
    return 1;
}

/*
 * Initialize the world
 */
static void
malloc_init ()
{
    char *p, b[64];
    int i, j;

#ifdef EXTRA_SANITY
    malloc_junk = 1;
#endif /* EXTRA_SANITY */

    for (i = 0; i < 3; i++) {
	if (i == 0) {
	    j = readlink("/etc/malloc.conf", b, sizeof b - 1);
	    if (j <= 0)
		continue;
	    b[j] = '\0';
	    p = b;
	} else if (i == 1 && issetugid() == 0) {
	    p = getenv("MALLOC_OPTIONS");
	} else if (i == 2) {
	    p = malloc_options;
	}
	else
	    p = NULL;
	for (; p && *p; p++) {
	    switch (*p) {
		case '>': malloc_cache   <<= 1; break;
		case '<': malloc_cache   >>= 1; break;
		case 'a': malloc_abort   = 0; break;
		case 'A': malloc_abort   = 1; break;
#ifdef MALLOC_STATS
		case 'd': malloc_stats   = 0; break;
		case 'D': malloc_stats   = 1; break;
#endif /* MALLOC_STATS */
#ifdef __FreeBSD__
		case 'h': malloc_hint    = 0; break;
		case 'H': malloc_hint    = 1; break;
#endif /* __FreeBSD__ */
		case 'r': malloc_realloc = 0; break;
		case 'R': malloc_realloc = 1; break;
		case 'j': malloc_junk    = 0; break;
		case 'J': malloc_junk    = 1; break;
#ifdef __FreeBSD__
		case 'u': malloc_utrace  = 0; break;
		case 'U': malloc_utrace  = 1; break;
#endif /* __FreeBSD__ */
		case 'z': malloc_zero    = 0; break;
		case 'Z': malloc_zero    = 1; break;
		default:
		    j = malloc_abort;
		    malloc_abort = 0;
		    wrtwarning("unknown char in MALLOC_OPTIONS\n");
		    malloc_abort = j;
		    break;
	    }
	}
    }

    UTRACE(0, 0, 0);

    /*
     * We want junk in the entire allocation, and zero only in the part
     * the user asked for.
     */
    if (malloc_zero)
	malloc_junk=1;

#ifdef EXTRA_SANITY
    if (malloc_stats)
	atexit(malloc_exit);
#endif /* EXTRA_SANITY */

#ifndef malloc_pagesize
    /* determine our pagesize */
    malloc_pagesize = getpagesize();
#endif /* malloc_pagesize */

#ifndef malloc_maxsize
    malloc_maxsize = malloc_pagesize >> 1;
#endif /* malloc_maxsize */

#ifndef malloc_pageshift
    {
    int i;
    /* determine how much we shift by to get there */
    for (i = malloc_pagesize; i > 1; i >>= 1)
	malloc_pageshift++;
    }
#endif /* malloc_pageshift */

#ifndef malloc_minsize
    {
    int i;
    /*
     * find the smallest size allocation we will bother about.
     * this is determined as the smallest allocation that can hold
     * it's own pginfo;
     */
    i = 2;
    for(;;) {
	int j;

	/* Figure out the size of the bits */
	j = malloc_pagesize/i;
	j /= 8;
	if (j < sizeof(u_long))
		j = sizeof (u_long);
	if (sizeof(struct pginfo) + j - sizeof (u_long) <= i)
		break;
	i += i;
    }
    malloc_minsize = i;
    }
#endif /* malloc_minsize */

    /* Allocate one page for the page directory */
    page_dir = (struct pginfo **) mmap(0, malloc_pagesize, PROT_READ|PROT_WRITE,
				       MAP_ANON|MAP_PRIVATE, -1, (off_t)0);
    if (page_dir == (struct pginfo **) -1)
	wrterror("mmap(2) failed, check limits.\n");

    /*
     * We need a maximum of malloc_pageshift buckets, steal these from the
     * front of the page_directory;
     */
    malloc_origo = ((u_long)pageround((u_long)sbrk(0))) >> malloc_pageshift;
    malloc_origo -= malloc_pageshift;

    malloc_ninfo = malloc_pagesize / sizeof *page_dir;

    /* Been here, done that */
    initialized++;

    /*
     * This is a nice hack from Kaleb Keithly (kaleb@x.org).
     * We can sbrk(2) further back when we keep this on a low address.
     */
    px = (struct pgfree *) imalloc (sizeof *px);

    if (!malloc_cache)
	malloc_cache++;

    malloc_cache <<= malloc_pageshift;
}

/*
 * Allocate a number of complete pages
 */
void *
malloc_pages(size)
    size_t size;
{
    void *p, *delay_free = 0;
    int i;
    struct pgfree *pf;
    u_long index;

    size = pageround(size);

    p = 0;
    /* Look for free pages before asking for more */
    for(pf = free_list.next; pf; pf = pf->next) {

#ifdef EXTRA_SANITY
	if (pf->size & malloc_pagemask)
	    wrterror("(ES): junk length entry on free_list\n");
	if (!pf->size)
	    wrterror("(ES): zero length entry on free_list\n");
	if (pf->page == pf->end)
	    wrterror("(ES): zero entry on free_list\n");
	if (pf->page > pf->end) 
	    wrterror("(ES): sick entry on free_list\n");
	if ((void*)pf->page >= (void*)sbrk(0))
	    wrterror("(ES): entry on free_list past brk\n");
	if (page_dir[ptr2index(pf->page)] != MALLOC_FREE) 
	    wrterror("(ES): non-free first page on free-list\n");
	if (page_dir[ptr2index(pf->end)-1] != MALLOC_FREE)
	    wrterror("(ES): non-free last page on free-list\n");
#endif /* EXTRA_SANITY */

	if (pf->size < size)
	    continue;

	if (pf->size == size) {
	    p = pf->page;
	    if (pf->next)
		    pf->next->prev = pf->prev;
	    pf->prev->next = pf->next;
	    delay_free = pf;
	    break;
	} 

	p = pf->page;
	pf->page = (char *)pf->page + size;
	pf->size -= size;
	break;
    }

#ifdef EXTRA_SANITY
    if (p && page_dir[ptr2index(p)] != MALLOC_FREE)
	wrterror("(ES): allocated non-free page on free-list\n");
#endif /* EXTRA_SANITY */

    size >>= malloc_pageshift;

    /* Map new pages */
    if (!p)
	p = map_pages(size);

    if (p) {

	index = ptr2index(p);
	page_dir[index] = MALLOC_FIRST;
	for (i=1;i<size;i++)
	    page_dir[index+i] = MALLOC_FOLLOW;

	if (malloc_junk)
	    memset(p, SOME_JUNK, size << malloc_pageshift);
    }

    if (delay_free) {
	if (!px)
	    px = delay_free;
	else
	    ifree(delay_free);
    }

    return p;
}

/*
 * Allocate a page of fragments
 */

static __inline int
malloc_make_chunks(bits)
    int bits;
{
    struct  pginfo *bp;
    void *pp;
    int i, k, l;

    /* Allocate a new bucket */
    pp = malloc_pages(malloc_pagesize);
    if (!pp)
	return 0;

    /* Find length of admin structure */
    l = sizeof *bp - sizeof(u_long);
    l += sizeof(u_long) *
	(((malloc_pagesize >> bits)+MALLOC_BITS-1) / MALLOC_BITS);

    /* Don't waste more than two chunks on this */
    if ((1UL<<(bits)) <= l+l) {
	bp = (struct  pginfo *)pp;
    } else {
	bp = (struct  pginfo *)imalloc(l);
	if (!bp)
	    return 0;
    }

    bp->size = (1UL<<bits);
    bp->shift = bits;
    bp->total = bp->free = malloc_pagesize >> bits;
    bp->page = pp;

    page_dir[ptr2index(pp)] = bp;

    bp->next = page_dir[bits];
    page_dir[bits] = bp;

    /* set all valid bits in the bits */
    k = bp->total;
    i = 0;

    /* Do a bunch at a time */
    for(;k-i >= MALLOC_BITS; i += MALLOC_BITS)
	bp->bits[i / MALLOC_BITS] = (u_long)~0;

    for(; i < k; i++)
	set_bit(bp, i);

    if (bp == bp->page) {
	/* Mark the ones we stole for ourselves */
	for(i=0;l > 0;i++) {
	    clr_bit(bp, i);
	    bp->free--;
	    bp->total--;
	    l -= (1 << bits);
	}
    }

    return 1;
}

/*
 * Allocate a fragment
 */
static void *
malloc_bytes(size)
    size_t size;
{
    int j;
    struct  pginfo *bp;
    int k;
    u_long *lp;

    /* Don't bother with anything less than this */
    if (size < malloc_minsize)
	size = malloc_minsize;

    /* Find the right bucket */
    j = fls((size)-1);

    /* If it's empty, make a page more of that size chunks */
    if (!page_dir[j] && !malloc_make_chunks(j))
	return 0;

    bp = page_dir[j];

    /* Find first word of bitmap which isn't empty */
    for (lp = bp->bits; !*lp; lp++)
	;

    /* Find that bit, and tweak it */
    k = ffs_ul(*lp) - 1;
    *lp ^= 1UL<<k;

    /* If there are no more free, remove from free-list */
    if (!--bp->free) {
	page_dir[j] = bp->next;
	bp->next = 0;
    }

    /* Adjust to the real offset of that chunk */
    k += (lp-bp->bits)*MALLOC_BITS;
    k <<= bp->shift;

    if (malloc_junk)
	memset((char *)bp->page + k, SOME_JUNK, bp->size);

    return (void *)((char *)bp->page + k);
}

/*
 * Allocate a piece of memory
 */
static void *
imalloc(size)
    size_t size;
{
    void *result;
#ifdef  _THREAD_SAFE
    int     status;
#endif

    if (!initialized)
	malloc_init();

    if (suicide)
	abort();

    if (size <= malloc_maxsize)
	result =  malloc_bytes(size);
    else
	result =  malloc_pages(size);

    if (malloc_abort && !result)
	wrterror("allocation failed.\n");

    if (malloc_zero)
	memset(result, 0, size);

    return result;
}

/*
 * Change the size of an allocation.
 */
static void *
irealloc(ptr, size)
    void *ptr;
    size_t size;
{
    void *p;
    u_long osize, index;
    struct pginfo **mp;
    int i;
#ifdef  _THREAD_SAFE
    int     status;
#endif

    if (suicide)
	return 0;

    if (!initialized) {
	wrtwarning("malloc() has never been called.\n");
	return 0;
    }

    index = ptr2index(ptr);

    if (index < malloc_pageshift) {
	wrtwarning("junk pointer, too low to make sense.\n");
	return 0;
    }

    if (index > last_index) {
	wrtwarning("junk pointer, too high to make sense.\n");
	return 0;
    }

    mp = &page_dir[index];

    if (*mp == MALLOC_FIRST) {			/* Page allocation */

	/* Check the pointer */
	if ((u_long)ptr & malloc_pagemask) {
	    wrtwarning("modified (page-) pointer.\n");
	    return 0;
	}

	/* Find the size in bytes */
	for (osize = malloc_pagesize; *++mp == MALLOC_FOLLOW;)
	    osize += malloc_pagesize;

        if (!malloc_realloc && 			/* unless we have to, */
	  size <= osize && 			/* .. or are too small, */
	  size > (osize - malloc_pagesize)) {	/* .. or can free a page, */
	    return ptr;				/* don't do anything. */
	}

    } else if (*mp >= MALLOC_MAGIC) {		/* Chunk allocation */

	/* Check the pointer for sane values */
	if (((u_long)ptr & ((*mp)->size-1))) {
	    wrtwarning("modified (chunk-) pointer.\n");
	    return 0;
	}

	/* Find the chunk index in the page */
	i = ((u_long)ptr & malloc_pagemask) >> (*mp)->shift;

	/* Verify that it isn't a free chunk already */
	if (tst_bit(*mp, i)) {
	    wrtwarning("chunk is already free.\n");
	    return 0;
	}

	osize = (*mp)->size;

	if (!malloc_realloc &&		/* Unless we have to, */
	  size < osize && 		/* ..or are too small, */
	  (size > osize/2 ||	 	/* ..or could use a smaller size, */
	  osize == malloc_minsize)) {	/* ..(if there is one) */
	    return ptr;			/* ..Don't do anything */
	}

    } else {
	wrtwarning("pointer to wrong page.\n");
	return 0;
    }

    p = imalloc(size);

    if (p) {
	/* copy the lesser of the two sizes, and free the old one */
	if (osize < size)
	    memcpy(p, ptr, osize);
	else
	    memcpy(p, ptr, size);
	ifree(ptr);
    } 
    return p;
}

/*
 * Free a sequence of pages
 */

static __inline void
free_pages(ptr, index, info)
    void *ptr;
    int index;
    struct pginfo *info;
{
    int i;
    struct pgfree *pf, *pt=0;
    u_long l;
    void *tail;

    if (info == MALLOC_FREE) {
	wrtwarning("page is already free.\n");
	return;
    }

    if (info != MALLOC_FIRST) {
	wrtwarning("pointer to wrong page.\n");
	return;
    }

    if ((u_long)ptr & malloc_pagemask) {
	wrtwarning("modified (page-) pointer.\n");
	return;
    }

    /* Count how many pages and mark them free at the same time */
    page_dir[index] = MALLOC_FREE;
    for (i = 1; page_dir[index+i] == MALLOC_FOLLOW; i++)
	page_dir[index + i] = MALLOC_FREE;

    l = i << malloc_pageshift;

#ifdef __FreeBSD__
    if (malloc_hint)
	madvise(ptr, l, MADV_FREE);
#endif

    tail = (char *)ptr+l;

    /* add to free-list */
    if (!px)
	px = imalloc(sizeof *pt);	/* This cannot fail... */
    px->page = ptr;
    px->end =  tail;
    px->size = l;
    if (!free_list.next) {

	/* Nothing on free list, put this at head */
	px->next = free_list.next;
	px->prev = &free_list;
	free_list.next = px;
	pf = px;
	px = 0;

    } else {

	/* Find the right spot, leave pf pointing to the modified entry. */
	tail = (char *)ptr+l;

	for(pf = free_list.next; pf->end < ptr && pf->next; pf = pf->next)
	    ; /* Race ahead here */

	if (pf->page > tail) {
	    /* Insert before entry */
	    px->next = pf;
	    px->prev = pf->prev;
	    pf->prev = px;
	    px->prev->next = px;
	    pf = px;
	    px = 0;
	} else if (pf->end == ptr ) {
	    /* Append to the previous entry */
	    pf->end = (char *)pf->end + l;
	    pf->size += l;
	    if (pf->next && pf->end == pf->next->page ) {
		/* And collapse the next too. */
		pt = pf->next;
		pf->end = pt->end;
		pf->size += pt->size;
		pf->next = pt->next;
		if (pf->next)
		    pf->next->prev = pf;
	    }
	} else if (pf->page == tail) {
	    /* Prepend to entry */
	    pf->size += l;
	    pf->page = ptr;
	} else if (!pf->next) {
	    /* Append at tail of chain */
	    px->next = 0;
	    px->prev = pf;
	    pf->next = px;
	    pf = px;
	    px = 0;
	} else {
	    wrterror("freelist is destroyed.\n");
	}
    }
    
    /* Return something to OS ? */
    if (!pf->next &&				/* If we're the last one, */
      pf->size > malloc_cache &&		/* ..and the cache is full, */
      pf->end == malloc_brk &&			/* ..and none behind us, */
      malloc_brk == sbrk(0)) {			/* ..and it's OK to do... */

	/*
	 * Keep the cache intact.  Notice that the '>' above guarantees that
	 * the pf will always have at least one page afterwards.
	 */
	pf->end = (char *)pf->page + malloc_cache;
	pf->size = malloc_cache;

	brk(pf->end);
	malloc_brk = pf->end;

	index = ptr2index(pf->end);
	last_index = index - 1;

	for(i=index;i <= last_index;)
	    page_dir[i++] = MALLOC_NOT_MINE;

	/* XXX: We could realloc/shrink the pagedir here I guess. */
    }
    if (pt)
	ifree(pt);
}

/*
 * Free a chunk, and possibly the page it's on, if the page becomes empty.
 */

/* ARGSUSED */
static __inline void
free_bytes(ptr, index, info)
    void *ptr;
    int index;
    struct pginfo *info;
{
    int i;
    struct pginfo **mp;
    void *vp;

    /* Find the chunk number on the page */
    i = ((u_long)ptr & malloc_pagemask) >> info->shift;

    if (((u_long)ptr & (info->size-1))) {
	wrtwarning("modified (chunk-) pointer.\n");
	return;
    }

    if (tst_bit(info, i)) {
	wrtwarning("chunk is already free.\n");
	return;
    }

    set_bit(info, i);
    info->free++;

    mp = page_dir + info->shift;

    if (info->free == 1) {

	/* Page became non-full */

	mp = page_dir + info->shift;
	/* Insert in address order */
	while (*mp && (*mp)->next && (*mp)->next->page < info->page)
	    mp = &(*mp)->next;
	info->next = *mp;
	*mp = info;
	return;
    }

    if (info->free != info->total)
	return;

    /* Find & remove this page in the queue */
    while (*mp != info) {
	mp = &((*mp)->next);
#ifdef EXTRA_SANITY
	if (!*mp)
		wrterror("(ES): Not on queue\n");
#endif /* EXTRA_SANITY */
    }
    *mp = info->next;

    /* Free the page & the info structure if need be */
    page_dir[ptr2index(info->page)] = MALLOC_FIRST;
    vp = info->page;		/* Order is important ! */
    if(vp != (void*)info) 
	ifree(info);
    ifree(vp);
}

static void
ifree(ptr)
    void *ptr;
{
    struct pginfo *info;
    int index;
#ifdef  _THREAD_SAFE
    int     status;
#endif

    /* This is legal */
    if (!ptr)
	return;

    if (!initialized) {
	wrtwarning("malloc() has never been called.\n");
	return;
    }

    /* If we're already sinking, don't make matters any worse. */
    if (suicide)
	return;

    index = ptr2index(ptr);

    if (index < malloc_pageshift) {
	wrtwarning("junk pointer, too low to make sense.\n");
	return;
    }

    if (index > last_index) {
	wrtwarning("junk pointer, too high to make sense.\n");
	return;
    }

    info = page_dir[index];

    if (info < MALLOC_MAGIC)
        free_pages(ptr, index, info);
    else
	free_bytes(ptr, index, info);
    return;
}

/*
 * These are the public exported interface routines.
 */

#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"
static int malloc_lock;
#define THREAD_LOCK() _thread_kern_sig_block(&malloc_lock);
#define THREAD_UNLOCK() _thread_kern_sig_unblock(&malloc_lock);
#else
#define THREAD_LOCK() 
#define THREAD_UNLOCK()
#endif

static int malloc_active;

void *
malloc(size_t size)
{
    register void *r;

    malloc_func = "malloc():";
    THREAD_LOCK();
    if (malloc_active++) {
	wrtwarning("recursive call.\n");
        malloc_active--;
	return (0);
    }
    r = imalloc(size);
    UTRACE(0, size, r);
    malloc_active--;
    THREAD_UNLOCK();
    return (r);
}

void
free(void *ptr)
{
    malloc_func = "free():";
    THREAD_LOCK();
    if (malloc_active++) {
	wrtwarning("recursive call.\n");
        malloc_active--;
	return;
    }
    ifree(ptr);
    UTRACE(ptr, 0, 0);
    malloc_active--;
    THREAD_UNLOCK();
    return;
}

void *
realloc(void *ptr, size_t size)
{
    register void *r;

    malloc_func = "realloc():";
    THREAD_LOCK();
    if (malloc_active++) {
	wrtwarning("recursive call.\n");
        malloc_active--;
	return (0);
    }
    if (!ptr) {
	r = imalloc(size);
    } else if (ptr && !size) {
	ifree(ptr);
	r = 0;
    } else {
        r = irealloc(ptr, size);
    }
    UTRACE(ptr, size, r);
    malloc_active--;
    THREAD_UNLOCK();
    return (r);
}
