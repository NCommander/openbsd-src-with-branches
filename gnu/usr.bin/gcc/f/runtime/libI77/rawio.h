#ifdef KR_headers
extern FILE *fdopen();
#else
#if defined (MSDOS) && !defined (GO32)
#include "io.h"
#define close _close
#define creat _creat
#define open _open
#define read _read
#define write _write
#endif
#ifdef __cplusplus
extern "C" {
#endif
#if !(defined (MSDOS) && !defined (GO32))
#ifdef OPEN_DECL
extern int creat(const char*,int), open(const char*,int);
#endif
extern int close(int);
extern int read(int,void*,size_t), write(int,void*,size_t);
extern int unlink(const char*);
#ifndef _POSIX_SOURCE
#ifndef NON_UNIX_STDIO
extern FILE *fdopen(int, const char*);
#endif
#endif
#endif

extern char *mktemp(char*);

#ifdef __cplusplus
	}
#endif
#endif

#ifndef NO_FCNTL
#include "fcntl.h"
#endif

#ifndef O_WRONLY
#define O_RDONLY 0
#define O_WRONLY 1
#endif
