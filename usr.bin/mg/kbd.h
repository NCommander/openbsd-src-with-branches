/*	$OpenBSD: kbd.h,v 1.11 2001/05/24 09:47:34 art Exp $	*/

/*
 * kbd.h: type definitions for symbol.c and kbd.c for mg experimental
 */

typedef struct {
	KCHAR	k_base;	/* first key in element		 */
	KCHAR	k_num;		/* last key in element		 */
	PF	*k_funcp;	/* pointer to array of pointers
					 * to functions */
	struct keymap_s *k_prefmap;	/* keymap of ONLY prefix key in
					 * element */
} MAP_ELEMENT;

/*
 * predefined keymaps are NOT type KEYMAP because final array needs
 * dimension.  If any changes are made to this struct, they must be reflected
 * in all keymap declarations.
 */

#define KEYMAPE(NUM)	{						\
	short	map_num;		/* elements used */		\
	short	map_max;		/* elements allocated */	\
	PF	map_default;		/* default function */		\
	MAP_ELEMENT map_element[NUM];	/* really [e_max] */		\
}
typedef struct keymap_s KEYMAPE(1) KEYMAP;

/* Number of map_elements to grow an overflowed keymap by */
#define IMAPEXT 0
#define MAPGROW 3
#define MAPINIT (MAPGROW+1)

/* Max number of default bindings added to avoid creating new element */
#define MAPELEDEF 4

typedef struct MAPS_S {
	KEYMAP	*p_map;
	char	*p_name;
	struct MAPS_S *p_next;
} MAPS;

extern MAPS	*maps;
extern MAPS	fundamental_mode;
#define		fundamental_map (fundamental_mode.p_map)

int	dobindkey		__P((KEYMAP *, char *, char *));
KEYMAP	*name_map		__P((char *));
MAPS	*name_mode		__P((char *));
PF	doscan			__P((KEYMAP *, int, KEYMAP **));
char	*map_name		__P((KEYMAP *));
void	maps_init(void);
int	maps_add(KEYMAP *, char *);

extern MAP_ELEMENT	*ele;
extern MAPS		*defb_modes[];
