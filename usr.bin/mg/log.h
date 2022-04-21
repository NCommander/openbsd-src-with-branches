/*      $OpenBSD: log.h,v 1.5 2019/07/18 10:50:24 lum Exp $   */

/* This file is in the public domain. */

/*
 * Specifically for mg logging functionality.
 *
 */
int	 mglog(PF, void *);
int	 mgloginit(void);
int	 mglog_execbuf(	const char* const,
			const char* const,
			const char* const,
			const char* const,
	     		const int,
			const int,
			const char* const,
			const char* const,
			const char* const
			);

int	 mglog_isvar(	const char* const,
			const char* const,
			const int
			);
int	 mglog_misc(	const char *, ...);

extern const char	*mglogpath_lines;
extern const char	*mglogpath_undo;
extern const char	*mglogpath_window;
extern const char	*mglogpath_key;
extern const char	*mglogpath_interpreter;
extern const char	*mglogpath_misc;
