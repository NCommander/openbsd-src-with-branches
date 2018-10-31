/* $OpenBSD$ */
/* crazy shit won't compile without the right defines */
extern void f
#if defined A
(
#endif
#if defined B
);
#endif
