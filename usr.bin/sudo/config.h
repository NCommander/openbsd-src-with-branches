/*	$OpenBSD$	*/

#ifndef _SUDO_CONFIG_H
#define _SUDO_CONFIG_H

/*
 * configure --prefix=/usr --with-devel --with-insults --with-bsdauth \
 *	     --with-env-editor --disable-path-info --with-logfac=authpriv
 */

#define HAVE_ASPRINTF 1
#define HAVE_BSD_AUTH_H 1
#define HAVE_CLOSEFROM 1
#define HAVE_DIRENT_H 1
#define HAVE_ERR_H 1
#define HAVE_FNMATCH 1
#define HAVE_FREEIFADDRS 1
#define HAVE_FSTAT 1
#define HAVE_GETCWD 1
#define HAVE_GETDOMAINNAME 1
#define HAVE_GETIFADDRS 1
#define HAVE_INITGROUPS 1
#define HAVE_INNETGR 1
#define HAVE_INTTYPES_H 1
#define HAVE_ISBLANK 1
#define HAVE_LOCKF 1
#define HAVE_LOGIN_CAP_H 1
#define HAVE_LONG_LONG 1
#define HAVE_LSEARCH 1
#define HAVE_MEMCHR 1
#define HAVE_MEMCPY 1
#define HAVE_MEMORY_H 1
#define HAVE_MEMSET 1
#define HAVE_NETGROUP_H 1
#define HAVE_PATHS_H 1
#define HAVE_SA_LEN 1
#define HAVE_SETRESUID 1
#define HAVE_SETRLIMIT 1
#define HAVE_SIGACTION 1
#define HAVE_SIG_ATOMIC_T 1
#define HAVE_SNPRINTF 1
#define HAVE_STDLIB_H 1
#define HAVE_STRCASECMP 1
#define HAVE_STRCHR 1
#define HAVE_STRERROR 1
#define HAVE_STRFTIME 1
#define HAVE_STRING_H 1
#define HAVE_STRLCAT 1
#define HAVE_STRLCPY 1
#define HAVE_STRRCHR 1
#define HAVE_SYS_SELECT_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_TERMIOS_H 1
#define HAVE_TZSET 1
#define HAVE_UNISTD_H 1
#define HAVE_UTIME 1
#define HAVE_UTIME_H 1
#define HAVE_UTIME_POSIX 1
#define HAVE_VASPRINTF 1
#define HAVE_VSNPRINTF 1
#define HAVE___PROGNAME 1

#define CLASSIC_INSULTS 1
#define CSOPS_INSULTS 1
#define DONT_LEAK_PATH_INFO 1
#define EDITOR _PATH_VI
#define ENV_EDITOR 1
#define INCORRECT_PASSWORD "Sorry, try again."
#define LOGFAC "authpriv"
#define LOGGING SLOG_SYSLOG
#define MAILSUBJECT "*** SECURITY information for %h ***"
#define MAILTO "root"
#define MAXLOGFILELEN 80
#define MAX_UID_T_LEN 10
#define PASSPROMPT "Password:"
#define PASSWORD_TIMEOUT 5
#define PRI_FAILURE "alert"
#define PRI_SUCCESS "notice"
#define RETSIGTYPE void
#define RUNAS_DEFAULT "root"
#define SEND_MAIL_WHEN_NO_USER 1
#define STDC_HEADERS 1
#define SUDO_UMASK 0022
#define	SUDOERS_UID 0
#define	SUDOERS_GID 0
#define	SUDOERS_MODE 0440
#define TIMEOUT 5
#define TRIES_FOR_PASSWORD 3
#define USE_INSULTS 1
#define VOID void
#define WITHOUT_PASSWD 1

#define sudo_waitpid(p, s, o)	waitpid(p, s, o)
#define	stat_sudoers	lstat
#define EXEC	execvp

#endif /* _SUDO_CONFIG_H */
