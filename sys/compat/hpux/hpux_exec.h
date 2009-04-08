/*	$OpenBSD$	*/

#if defined(__hppa__)
#include <compat/hpux/hppa/hpux_exec.h>
#elif defined(__m68k__)
#include <compat/hpux/m68k/hpux_exec.h>
#else
#error COMPILING FOR UNSUPPORTED ARCHITECTURE
#endif
