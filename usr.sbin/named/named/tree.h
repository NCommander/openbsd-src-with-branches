/*	$OpenBSD: tree.h,v 1.4 2002/02/16 21:28:06 millert Exp $	*/

/* tree.h - declare structures used by tree library
 *
 * vix 22jan93 [revisited; uses RCS, ANSI, POSIX; has bug fixes]
 * vix 27jun86 [broken out of tree.c]
 *
 * $From: tree.h,v 8.1 1994/12/15 06:24:14 vixie Exp $
 */


#ifndef	_TREE_H_INCLUDED
#define	_TREE_H_INCLUDED


/*
 * tree_t is our package-specific anonymous pointer.
 */
#if defined(__STDC__) || defined(__GNUC__)
typedef	void *tree_t;
#else
typedef	char *tree_t;
#endif


typedef	struct tree_s {
		tree_t		data;
		struct tree_s	*left, *right;
		short		bal;
	}
	tree;


void	tree_init(tree **);
tree_t	tree_srch(tree **, int (*)(), tree_t);
tree_t	tree_add(tree **, int (*)(), tree_t, void (*)());
int	tree_delete(tree **, int (*)(), tree_t, void (*)());
int	tree_trav(tree **, int (*)());
void	tree_mung(tree **, void (*)());


#endif	/* _TREE_H_INCLUDED */
