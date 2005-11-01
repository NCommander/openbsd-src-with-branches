/*	$OpenBSD$	*/

#if defined(__hppa__)
#include <compat/hpux/hppa/hpux_syscalls.c>
#elif defined(__m68k__)
#include <compat/hpux/m68k/hpux_syscalls.c>
#else
#error COMPILING FOR UNSUPPORTED ARCHITECTURE
#endif
