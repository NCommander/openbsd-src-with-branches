/* $OpenBSD: ld.h,v 1.2 2000/07/08 19:51:46 rahnds Exp $ */
/*
 * Header file to make code compatible with ELF version 
 * ldconfig was taken from the a.out ld.
 */
#include <link.h>

extern int	n_search_dirs;
extern char	**search_dirs;
char	*xstrdup __P((char *));
void	*xmalloc __P((size_t));
void	*xrealloc __P((void *, size_t));
char	*concat __P((const char *, const char *, const char *));

#define PAGSIZ	__LDPGSZ
