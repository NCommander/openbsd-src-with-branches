/* $OpenBSD: tsort.c,v 1.36 2017/05/20 09:31:19 espie Exp $ */
/* ex:ts=8 sw=4:
 *
 * Copyright (c) 1999-2004 Marc Espie <espie@openbsd.org>
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

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ohash.h>

/* The complexity of topological sorting is O(e), where e is the
 * size of input.  While reading input, vertices have to be identified,
 * thus add the complexity of e keys retrieval among v keys using
 * an appropriate data structure.  This program uses open double hashing
 * for that purpose.  See Knuth for the expected complexity of double
 * hashing (Brent variation should probably be used if v << e, as a user
 * option).
 *
 * The algorithm used for longest cycle reporting is accurate, but somewhat
 * expensive.  It may need to build all free paths of the graph (a free
 * path is a path that never goes twice through the same node), whose
 * number can be as high as O(2^e).  Usually, the number of free paths is
 * much smaller though.  This program's author does not believe that a
 * significantly better worst-case complexity algorithm exists.
 *
 * In case of a hints file, the set of minimal nodes is maintained as a
 * heap.  The resulting complexity is O(e+v log v) for the worst case.
 * The average should actually be near O(e).
 *
 * If the hints file is incomplete, there is some extra complexity incurred
 * by make_transparent, which does propagate order values to unmarked
 * nodes. In the worst case, make_transparent is  O(e u),
 * where u is the number of originally unmarked nodes.
 * In practice, it is much faster.
 *
 * The simple topological sort algorithm detects cycles.  This program
 * goes further, breaking cycles through the use of simple heuristics.
 * Each cycle break checks the whole set of nodes, hence if c cycles break
 * are needed, this is an extra cost of O(c v).
 *
 * Possible heuristics are as follows:
 * - break cycle at node with lowest number of predecessors (default case),
 * - break longest cycle at node with lowest number of predecessors,
 * - break cycle at next node from the hints file.
 *
 * Except for the hints file case, which sets an explicit constraint on
 * which cycle to break, those heuristics locally result in the smallest
 * number of broken edges.
 *
 * Those are admittedly greedy strategies, as is the selection of the next
 * node from the hints file amongst equivalent candidates that is used for
 * `stable' topological sorting.
 */

#ifdef __GNUC__
#define UNUSED	__attribute__((unused))
#else
#define UNUSED
#endif

struct node;

/* The set of arcs from a given node is stored as a linked list.  */
struct link {
	struct link *next;
	struct node *node;
};

#define NO_ORDER	UINT_MAX

struct node {
	unsigned int refs;	/* Number of arcs left, coming into this node.
				 * Note that nodes with a null count can't
				 * be part of cycles.  */
	unsigned int order; 	/* Order of nodes according to a hint file.  */

	struct link  *arcs;	/* List of forward arcs.  */

	/* Cycle detection algorithms build a free path of nodes.  */
	struct node  *from; 	/* Previous node in the current path.  */
	struct link  *traverse;	/* Next link to traverse when backtracking.  */
	unsigned int mark;	/* Mark processed nodes in cycle discovery.  */

	char         k[1];	/* Name of this node.  */
};

#define HASH_START 9

struct array {
	unsigned int entries;
	struct node  **t;
};

static void nodes_init(struct ohash *);
static struct node *node_lookup(struct ohash *, const char *, const char *);
static __dead void usage(void);
static struct node *new_node(const char *, const char *);

static unsigned int read_pairs(FILE *, struct ohash *, int,
    const char *, unsigned int, int);
static void split_nodes(struct ohash *, struct array *, struct array *);
static void make_transparent(struct ohash *);
static void insert_arc(struct node *, struct node *);

#ifdef DEBUG
static void dump_node(struct node *);
static void dump_array(struct array *);
static void dump_hash(struct ohash *);
#endif
static unsigned int read_hints(FILE *, struct ohash *, int,
    const char *, unsigned int);
static struct node *find_smallest_node(struct array *);
static struct node *find_good_cycle_break(struct array *);
static void print_cycle(struct array *);
static struct node *find_cycle_from(struct node *, struct array *);
static struct node *find_predecessor(struct array *, struct node *);
static unsigned int traverse_node(struct node *, unsigned int, struct array *);
static struct node *find_longest_cycle(struct array *, struct array *);
static struct node *find_normal_cycle(struct array *, struct array *);

static void heap_down(struct array *, unsigned int);
static void heapify(struct array *, int);
static struct node *dequeue(struct array *);
static void enqueue(struct array *, struct node *);



static void *hash_calloc(size_t, size_t, void *);
static void hash_free(void *, void *);
static void* entry_alloc(size_t, void *);
static void *ereallocarray(void *, size_t, size_t);
static void *emem(void *);
#define DEBUG_TRAVERSE 0
static struct ohash_info node_info = {
	offsetof(struct node, k), NULL, hash_calloc, hash_free, entry_alloc };
static void parse_args(int, char *[], struct ohash *);
static int tsort(struct ohash *);

static int 		quiet_flag, long_flag,
			warn_flag, hints_flag, verbose_flag;


int main(int, char *[]);

/***
 *** Memory handling.
 ***/

static void *
emem(void *p)
{
	if (p)
		return p;
	else
		errx(1, "Memory exhausted");
}

static void *
hash_calloc(size_t n, size_t s, void *u UNUSED)
{
	return emem(calloc(n, s));
}

static void
hash_free(void *p, void *u UNUSED)
{
	free(p);
}

static void *
entry_alloc(size_t s, void *u UNUSED)
{
	return ereallocarray(NULL, 1, s);
}

static void *
ereallocarray(void *p, size_t n, size_t s)
{
	return emem(reallocarray(p, n, s));
}


/***
 *** Hash table.
 ***/

/* Inserting and finding nodes in the hash structure.
 * We handle interval strings for efficiency wrt fgetln.  */
static struct node *
new_node(const char *start, const char *end)
{
	struct node 	*n;

	n = ohash_create_entry(&node_info, start, &end);
	n->from = NULL;
	n->arcs = NULL;
	n->refs = 0;
	n->mark = 0;
	n->order = NO_ORDER;
	n->traverse = NULL;
	return n;
}


static void
nodes_init(struct ohash *h)
{
	ohash_init(h, HASH_START, &node_info);
}

static struct node *
node_lookup(struct ohash *h, const char *start, const char *end)
{
	unsigned int	i;
	struct node *	n;

	i = ohash_qlookupi(h, start, &end);

	n = ohash_find(h, i);
	if (n == NULL)
		n = ohash_insert(h, i, new_node(start, end));
	return n;
}

#ifdef DEBUG
static void
dump_node(struct node *n)
{
	struct link 	*l;

	if (n->refs == 0)
		return;
	printf("%s (%u/%u): ", n->k, n->order, n->refs);
	for (l = n->arcs; l != NULL; l = l->next)
		if (n->refs != 0)
		printf("%s(%u/%u) ", l->node->k, l->node->order, l->node->refs);
    	putchar('\n');
}

static void
dump_array(struct array *a)
{
	unsigned int 	i;

	for (i = 0; i < a->entries; i++)
		dump_node(a->t[i]);
}

static void
dump_hash(struct ohash *h)
{
	unsigned int 	i;
	struct node 	*n;

	for (n = ohash_first(h, &i); n != NULL; n = ohash_next(h, &i))
		dump_node(n);
}
#endif

/***
 *** Reading data.
 ***/

static void
insert_arc(struct node *a, struct node *b)
{
	struct link 	*l;

	/* Check that this arc is not already present.  */
	for (l = a->arcs; l != NULL; l = l->next) {
		if (l->node == b)
			return;
	}
	b->refs++;
	l = ereallocarray(NULL, 1, sizeof(struct link));
	l->node = b;
	l->next = a->arcs;
	a->arcs = l;
}

static unsigned int
read_pairs(FILE *f, struct ohash *h, int reverse, const char *name,
    unsigned int order, int hint)
{
	int 		toggle;
	struct node 	*a;
	size_t 		size;
	char 		*str;

	toggle = 1;
	a = NULL;

	while ((str = fgetln(f, &size)) != NULL) {
		char *sentinel;

		sentinel = str + size;
		for (;;) {
			char *e;

			while (str < sentinel &&
			    isspace((unsigned char)*str))
				str++;
			if (str == sentinel)
				break;
			for (e = str;
			    e < sentinel && !isspace((unsigned char)*e); e++)
				continue;
			if (toggle) {
				a = node_lookup(h, str, e);
				if (a->order == NO_ORDER && hint)
					a->order = order++;
			} else {
				struct node *b;

				b = node_lookup(h, str, e);
				assert(a != NULL);
				if (b != a) {
					if (reverse)
						insert_arc(b, a);
					else
						insert_arc(a, b);
				}
			}
			toggle = !toggle;
			str = e;
		}
	}
	if (toggle == 0)
		errx(1, "odd number of node names in %s", name);
    	if (!feof(f))
		err(1, "error reading %s", name);
	return order;
}

static unsigned int
read_hints(FILE *f, struct ohash *h, int quiet, const char *name,
    unsigned int order)
{
	char 		*str;
	size_t 		size;

	while ((str = fgetln(f, &size)) != NULL) {
		char *sentinel;

		sentinel = str + size;
		for (;;) {
			char *e;
			struct node *a;

			while (str < sentinel && isspace((unsigned char)*str))
				str++;
			if (str == sentinel)
				break;
			for (e = str;
			    e < sentinel && !isspace((unsigned char)*e); e++)
				continue;
			a = node_lookup(h, str, e);
			if (a->order != NO_ORDER) {
				if (!quiet)
				    warnx(
					"duplicate node %s in hints file %s",
					a->k, name);
			} else
				a->order = order++;
			str = e;
		}
	}
    	if (!feof(f))
		err(1, "error reading %s", name);
	return order;
}


/***
 *** Standard heap handling routines.
 ***/

static void
heap_down(struct array *h, unsigned int i)
{
	unsigned int 	j;
	struct node 	*swap;

	for (; (j=2*i+1) < h->entries; i = j) {
		if (j+1 < h->entries && h->t[j+1]->order < h->t[j]->order)
		    	j++;
		if (h->t[i]->order <= h->t[j]->order)
			break;
		swap = h->t[i];
		h->t[i] = h->t[j];
		h->t[j] = swap;
	}
}

static void
heapify(struct array *h, int verbose)
{
	unsigned int 	i;

	for (i = h->entries; i != 0;) {
		if (h->t[--i]->order == NO_ORDER && verbose)
			warnx("node %s absent from hints file", h->t[i]->k);
		heap_down(h, i);
	}
}

#define DEQUEUE(h) ( hints_flag ? dequeue(h) : (h)->t[--(h)->entries] )

static struct node *
dequeue(struct array *h)
{
	struct node 	*n;

	if (h->entries == 0)
		n = NULL;
	else {
		n = h->t[0];
		if (--h->entries != 0) {
		    h->t[0] = h->t[h->entries];
		    heap_down(h, 0);
		}
	}
	return n;
}

#define ENQUEUE(h, n) do {			\
	if (hints_flag)				\
		enqueue((h), (n));		\
	else					\
		(h)->t[(h)->entries++] = (n);	\
	} while(0);

static void
enqueue(struct array *h, struct node *n)
{
	unsigned int 	i, j;
	struct node 	*swap;

	h->t[h->entries++] = n;
	for (i = h->entries-1; i > 0; i = j) {
		j = (i-1)/2;
		if (h->t[j]->order < h->t[i]->order)
			break;
		swap = h->t[j];
		h->t[j] = h->t[i];
		h->t[i] = swap;
	}
}

/* Nodes without order should not hinder direct dependencies.
 * Iterate until no nodes are left.
 */
static void
make_transparent(struct ohash *hash)
{
	struct node 	*n;
	unsigned int 	i;
	struct link 	*l;
	int		adjusted;
	int		bad;
	unsigned int	min;

	/* first try to solve complete nodes */
	do {
		adjusted = 0;
		bad = 0;
		for (n = ohash_first(hash, &i); n != NULL;
		    n = ohash_next(hash, &i)) {
			if (n->order == NO_ORDER) {
				min = NO_ORDER;

				for (l = n->arcs; l != NULL; l = l->next) {
					/* unsolved node -> delay resolution */
					if (l->node->order == NO_ORDER) {
						bad = 1;
						break;
					} else if (l->node->order < min)
						min = l->node->order;
				}
				if (min < NO_ORDER && l == NULL) {
					n->order = min;
					adjusted = 1;
				}
			}
		}

	} while (adjusted);

	/* then, if incomplete nodes are left, do them */
	if (bad) do {
		adjusted = 0;
		for (n = ohash_first(hash, &i); n != NULL;
		    n = ohash_next(hash, &i))
			if (n->order == NO_ORDER)
				for (l = n->arcs; l != NULL; l = l->next)
					if (l->node->order < n->order) {
						n->order = l->node->order;
						adjusted = 1;
					}
	} while (adjusted);
}


/***
 *** Search through hash array for nodes.
 ***/

/* Split nodes into unrefed nodes/live nodes.  */
static void
split_nodes(struct ohash *hash, struct array *heap, struct array *remaining)
{

	struct node *n;
	unsigned int i;

	heap->t = ereallocarray(NULL, ohash_entries(hash),
	    sizeof(struct node *));
	remaining->t = ereallocarray(NULL, ohash_entries(hash),
	    sizeof(struct node *));
	heap->entries = 0;
	remaining->entries = 0;

	for (n = ohash_first(hash, &i); n != NULL; n = ohash_next(hash, &i)) {
		if (n->refs == 0)
			heap->t[heap->entries++] = n;
		else
			remaining->t[remaining->entries++] = n;
	}
}

/* Good point to break a cycle: live node with as few refs as possible. */
static struct node *
find_good_cycle_break(struct array *h)
{
	unsigned int 	i;
	unsigned int 	best;
	struct node 	*u;

	best = UINT_MAX;
	u = NULL;

	assert(h->entries != 0);
	for (i = 0; i < h->entries; i++) {
		struct node *n = h->t[i];
		/* No need to look further. */
		if (n->refs == 1)
			return n;
		if (n->refs != 0 && n->refs < best) {
			best = n->refs;
			u = n;
		}
	}
	assert(u != NULL);
	return u;
}

/*  Retrieve the node with the smallest order.  */
static struct node *
find_smallest_node(struct array *h)
{
	unsigned int 	i;
	unsigned int 	best;
	struct node 	*u;

	best = UINT_MAX;
	u = NULL;

	assert(h->entries != 0);
	for (i = 0; i < h->entries; i++) {
		struct node *n = h->t[i];
		if (n->refs != 0 && n->order < best) {
			best = n->order;
			u = n;
		}
	}
	assert(u != NULL);
	return u;
}


/***
 *** Graph algorithms.
 ***/

/* Explore the nodes reachable from i to find a cycle, store it in c.
 * This may fail.  */
static struct node *
find_cycle_from(struct node *i, struct array *c)
{
	struct node 	*n;

	n = i;
	/* XXX Previous cycle findings may have left this pointer non-null.  */
	i->from = NULL;

	for (;;) {
		/* Note that all marks are reversed before this code exits.  */
		n->mark = 1;
		if (n->traverse)
			n->traverse = n->traverse->next;
		else
			n->traverse = n->arcs;
		/* Skip over dead nodes.  */
		while (n->traverse && n->traverse->node->refs == 0)
			n->traverse = n->traverse->next;
		if (n->traverse) {
			struct node *go = n->traverse->node;

			if (go->mark) {
				c->entries = 0;
				for (; n != NULL && n != go; n = n->from) {
					c->t[c->entries++] = n;
					n->mark = 0;
				}
				for (; n != NULL; n = n->from)
					n->mark = 0;
				c->t[c->entries++] = go;
				return go;
			} else {
			    go->from = n;
			    n = go;
			}
		} else {
			n->mark = 0;
			n = n->from;
			if (n == NULL)
				return NULL;
		}
	}
}

/* Find a live predecessor of node n.  This is a slow routine, as it needs
 * to go through the whole array, but it is not needed often.
 */
static struct node *
find_predecessor(struct array *a, struct node *n)
{
	unsigned int i;

	for (i = 0; i < a->entries; i++) {
		struct node *m;

		m = a->t[i];
		if (m->refs != 0) {
			struct link *l;

			for (l = m->arcs; l != NULL; l = l->next)
				if (l->node == n)
					return m;
		}
	}
	assert(1 == 0);
	return NULL;
}

/* Traverse all strongly connected components reachable from node n.
   Start numbering them at o. Return the maximum order reached.
   Update the largest cycle found so far.
 */
static unsigned int
traverse_node(struct node *n, unsigned int o, struct array *c)
{
	unsigned int 	min, max;

	n->from = NULL;
	min = o;
	max = ++o;

	for (;;) {
		n->mark = o;
		if (DEBUG_TRAVERSE)
			printf("%s(%u) ", n->k, n->mark);
		/* Find next arc to explore.  */
		if (n->traverse)
			n->traverse = n->traverse->next;
		else
			n->traverse = n->arcs;
		/* Skip over dead nodes.  */
		while (n->traverse && n->traverse->node->refs == 0)
			n->traverse = n->traverse->next;
		/* If arc left.  */
		if (n->traverse) {
			struct node 	*go;

			go = n->traverse->node;
			/* Optimisation: if go->mark < min, we already
			 * visited this strongly-connected component in
			 * a previous pass.  Hence, this can yield no new
			 * cycle.  */

			/* Not part of the current path: go for it.  */
			if (go->mark == 0 || go->mark == min) {
				go->from = n;
				n = go;
				o++;
				if (o > max)
					max = o;
			/* Part of the current path: check cycle length.  */
			} else if (go->mark > min) {
				if (DEBUG_TRAVERSE)
					printf("%d\n", o - go->mark + 1);
				if (o - go->mark + 1 > c->entries) {
					struct node *t;
					unsigned int i;

					c->entries = o - go->mark + 1;
					i = 0;
					c->t[i++] = go;
					for (t = n; t != go; t = t->from)
						c->t[i++] = t;
				}
			}

		/* No arc left: backtrack.  */
		} else {
			n->mark = min;
			n = n->from;
			if (!n)
				return max;
			o--;
		}
	}
}

static void
print_cycle(struct array *c)
{
	unsigned int 	i;

	/* Printing in reverse order, since cycle discoveries finds reverse
	 * edges.  */
	for (i = c->entries; i != 0;) {
		i--;
		warnx("%s", c->t[i]->k);
	}
}

static struct node *
find_longest_cycle(struct array *h, struct array *c)
{
	unsigned int 	i;
	unsigned int 	o;
	unsigned int 	best;
	struct node 	*n;
	static int 	notfirst = 0;

	assert(h->entries != 0);

	/* No cycle found yet.  */
	c->entries = 0;

	/* Reset the set of marks, except the first time around.  */
	if (notfirst) {
		for (i = 0; i < h->entries; i++)
			h->t[i]->mark = 0;
	} else
		notfirst = 1;

	o = 0;

	/* Traverse the array.  Each unmarked, live node heralds a
	 * new set of strongly connected components.  */
	for (i = 0; i < h->entries; i++) {
		n = h->t[i];
		if (n->refs != 0 && n->mark == 0) {
			/* Each call to traverse_node uses a separate
			 * interval of numbers to mark nodes.  */
			o++;
			o = traverse_node(n, o, c);
		}
	}

	assert(c->entries != 0);
	n = c->t[0];
	best = n->refs;
	for (i = 0; i < c->entries; i++) {
		if (c->t[i]->refs < best) {
			n = c->t[i];
			best = n->refs;
		}
	}
	return n;
}

static struct node *
find_normal_cycle(struct array *h, struct array *c)
{
	struct node *b, *n;

	if (hints_flag)
		n = find_smallest_node(h);
	else
		n = find_good_cycle_break(h);
	while ((b = find_cycle_from(n, c)) == NULL)
		n = find_predecessor(h, n);
	return b;
}


#define plural(n) ((n) > 1 ? "s" : "")

static void 
parse_args(int argc, char *argv[], struct ohash *pairs)
{
	int c;
	unsigned int	order;
	int reverse_flag;
	const char **files;
	int i, j;

	i = 0;

	reverse_flag = quiet_flag = long_flag =
		warn_flag = hints_flag = verbose_flag = 0;
	/* argc is good enough, as we start at argv[1] */
	files = ereallocarray(NULL, argc, sizeof (char *));
	while ((c = getopt(argc, argv, "h:flqrvw")) != -1) {
		switch(c) {
		case 'h':
			files[i++] = optarg;
			hints_flag = 1;
			break;
			/*FALLTHRU*/
		case 'f':
			hints_flag = 2;
			break;
		case 'l':
			long_flag = 1;
			break;
		case 'q':
			quiet_flag = 1;
			break;
		case 'r':
			reverse_flag = 1;
			break;
		case 'v':
			verbose_flag = 1;
			break;
		case 'w':
			warn_flag = 1;
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	switch(argc) {
	case 1:
		files[i++] = argv[0];
		break;
	case 0:
		break;
	default:
		usage();
	}

	files[i] = NULL;

	nodes_init(pairs);
	order = 0;
		
	for (j = 0; j != i-argc; j++) {
		FILE *f;

		f = fopen(files[j], "r");
		if (f == NULL)
			err(1, "Can't open hint file %s", files[i]);
		order = read_hints(f, pairs, quiet_flag, files[i], order);
		fclose(f);
    	}
	free(files);

	if (argc == 1) {
		FILE *f;

		f = fopen(argv[0], "r");
		if (f == NULL)
			err(1, "Can't open file %s", argv[0]);
		order = read_pairs(f, pairs, reverse_flag, argv[0], order,
		    hints_flag == 2);
		fclose(f);
	} else {
		order = read_pairs(stdin, pairs, reverse_flag, "stdin",
		    order, hints_flag == 2);
	}
}

static int
tsort(struct ohash *pairs)
{
	    struct array 	aux;	/* Unrefed nodes/cycle reporting.  */
	    struct array	remaining;
	    unsigned int	broken_arcs, broken_cycles;
	    unsigned int	left;

	    broken_arcs = 0;
	    broken_cycles = 0;

	    if (hints_flag)
		    make_transparent(pairs);
	    split_nodes(pairs, &aux, &remaining);
	    ohash_delete(pairs);

	    if (hints_flag)
		    heapify(&aux, verbose_flag);

	    left = remaining.entries + aux.entries;
	    while (left != 0) {

		    /* Standard topological sort.  */
		    while (aux.entries) {
			    struct link *l;
			    struct node *n;

			    n = DEQUEUE(&aux);
			    printf("%s\n", n->k);
			    left--;
			    /* We can't free nodes, as we don't know which
			     * entry we can remove in the hash table.  We
			     * rely on refs == 0 to recognize live nodes.
			     * Decrease ref count of live nodes, enter new
			     * candidates into the unrefed list.  */
			    for (l = n->arcs; l != NULL; l = l->next)
				    if (l->node->refs != 0 &&
					--l->node->refs == 0) {
					    ENQUEUE(&aux, l->node);
				    }
		    }
		    /* There are still cycles to break.  */
		    if (left != 0) {
			    struct node *n;

			    broken_cycles++;
			    /* XXX Simple cycle detection and long cycle
			     * detection are mutually exclusive.  */

			    if (long_flag)
				    n = find_longest_cycle(&remaining, &aux);
			    else
				    n = find_normal_cycle(&remaining, &aux);

			    if (!quiet_flag) {
				    warnx("cycle in data");
				    print_cycle(&aux);
			    }

			    if (verbose_flag)
				    warnx("%u edge%s broken", n->refs,
					plural(n->refs));
			    broken_arcs += n->refs;
			    n->refs = 0;
			    /* Reinitialization, cycle reporting uses aux.  */
			    aux.t[0] = n;
			    aux.entries = 1;
		    }
	    }
	    if (verbose_flag && broken_cycles != 0)
		    warnx("%u cycle%s broken, for a total of %u edge%s",
			broken_cycles, plural(broken_cycles),
			broken_arcs, plural(broken_arcs));
	    if (warn_flag)
		    return (broken_cycles < 256 ? broken_cycles : 255);
	    else
		    return (0);
}

int
main(int argc, char *argv[])
{
	struct ohash 	pairs;

	if (pledge("stdio rpath", NULL) == -1)
		err(1, "pledge");

	parse_args(argc, argv, &pairs);

	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

	return tsort(&pairs);
}


extern char *__progname;

static void
usage(void)
{
	fprintf(stderr, "Usage: %s [-flqrvw] [-h file] [file]\n", __progname);
	exit(1);
}
