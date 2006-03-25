/*	$OpenBSD$	*/

#if defined(__hppa__)
#include <compat/hpux/hppa/hpux_syscall.h>
#elif defined(__m68k__)
#include <compat/hpux/m68k/hpux_syscall.h>
#else
#error COMPILING FOR UNSUPPORTED ARCHITECTURE
#endif
