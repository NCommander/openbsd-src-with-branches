#ifndef EXPANDCHILDREN_H
#define EXPANDCHILDREN_H
/*	$OpenBSD: suff.h,v 1.10 2012/12/06 14:30:35 espie Exp $ */

extern void LinkParent(GNode *, GNode *);

/* partial expansion of children. */
extern void expand_children_from(GNode *, LstNode);
/* expand_all_children(gn):
 *	figure out all variable/wildcards expansions in gn.
 *	TODO pretty sure this is independent from the main suff module.
 */
#define expand_all_children(gn)	\
    expand_children_from(gn, Lst_First(&(gn)->children))

#endif
