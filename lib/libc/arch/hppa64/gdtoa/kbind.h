/* $OpenBSD$ */

/* kbind disabled in the kernel for hppa64 until we do dynamic linking */
#define	MD_DISABLE_KBIND	do { } while (0)
