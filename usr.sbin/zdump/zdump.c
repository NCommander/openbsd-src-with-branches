/*	$OpenBSD: zdump.c,v 1.7 2015/02/09 22:49:10 tedu Exp $ */
/*
** This file is in the public domain, so clarified as of
** 2009-05-17 by Arthur David Olson.
*/

/*
** This code has been made independent of the rest of the time
** conversion package to increase confidence in the verification it provides.
** You can use this code to help in verifying other implementations.
*/

#include <ctype.h>	/* for isalpha et al. */
#include <float.h>	/* for FLT_MAX and DBL_MAX */
#include <stdio.h>	/* for stdout, stderr, perror */
#include <string.h>	/* for strlcpy */
#include <stdlib.h>	/* for exit, malloc, atoi */
#include <time.h>	/* for struct tm */

#ifndef ZDUMP_LO_YEAR
#define ZDUMP_LO_YEAR	(-500)
#endif /* !defined ZDUMP_LO_YEAR */

#ifndef ZDUMP_HI_YEAR
#define ZDUMP_HI_YEAR	2500
#endif /* !defined ZDUMP_HI_YEAR */

#ifndef MAX_STRING_LENGTH
#define MAX_STRING_LENGTH	1024
#endif /* !defined MAX_STRING_LENGTH */

#ifndef TRUE
#define TRUE		1
#endif /* !defined TRUE */

#ifndef FALSE
#define FALSE		0
#endif /* !defined FALSE */

#ifndef SECSPERMIN
#define SECSPERMIN	60
#endif /* !defined SECSPERMIN */

#ifndef MINSPERHOUR
#define MINSPERHOUR	60
#endif /* !defined MINSPERHOUR */

#ifndef SECSPERHOUR
#define SECSPERHOUR	(SECSPERMIN * MINSPERHOUR)
#endif /* !defined SECSPERHOUR */

#ifndef HOURSPERDAY
#define HOURSPERDAY	24
#endif /* !defined HOURSPERDAY */

#ifndef EPOCH_YEAR
#define EPOCH_YEAR	1970
#endif /* !defined EPOCH_YEAR */

#ifndef TM_YEAR_BASE
#define TM_YEAR_BASE	1900
#endif /* !defined TM_YEAR_BASE */

#ifndef DAYSPERNYEAR
#define DAYSPERNYEAR	365
#endif /* !defined DAYSPERNYEAR */

#ifndef isleap
#define isleap(y) (((y) % 4) == 0 && (((y) % 100) != 0 || ((y) % 400) == 0))
#endif /* !defined isleap */

#ifndef isleap_sum
/*
** See tzfile.h for details on isleap_sum.
*/
#define isleap_sum(a, b)	isleap((a) % 400 + (b) % 400)
#endif /* !defined isleap_sum */

#define SECSPERDAY	((long) SECSPERHOUR * HOURSPERDAY)
#define SECSPERNYEAR	(SECSPERDAY * DAYSPERNYEAR)
#define SECSPERLYEAR	(SECSPERNYEAR + SECSPERDAY)

#define _(msgid) msgid

#ifndef TZ_DOMAIN
#define TZ_DOMAIN "tz"
#endif /* !defined TZ_DOMAIN */

extern char **	environ;
extern int	getopt(int argc, char * const argv[],
			const char * options);
extern char *	optarg;
extern int	optind;
extern char *	tzname[2];

static time_t	absolute_min_time;
static time_t	absolute_max_time;
static size_t	longest;
static char *	progname;
static int	warned;

static char *	abbr(struct tm * tmp);
static void	abbrok(const char * abbrp, const char * zone);
static long	delta(struct tm * newp, struct tm * oldp);
static void	dumptime(const struct tm * tmp);
static time_t	hunt(char * name, time_t lot, time_t	hit);
static void	setabsolutes(void);
static void	show(char * zone, time_t t, int v);
static const char *	tformat(void);
static time_t	yeartot(long y);

#ifndef TYPECHECK
#define my_localtime	localtime
#else /* !defined TYPECHECK */
static struct tm *
my_localtime(tp)
time_t *	tp;
{
	struct tm *	tmp;

	tmp = localtime(tp);
	if (tp != NULL && tmp != NULL) {
		struct tm	tm;
		time_t	t;

		tm = *tmp;
		t = mktime(&tm);
		if (t - *tp >= 1 || *tp - t >= 1) {
			fflush(stdout);
			fprintf(stderr, "\n%s: ", progname);
			fprintf(stderr, tformat(), *tp);
			fprintf(stderr, " ->");
			fprintf(stderr, " year=%d", tmp->tm_year);
			fprintf(stderr, " mon=%d", tmp->tm_mon);
			fprintf(stderr, " mday=%d", tmp->tm_mday);
			fprintf(stderr, " hour=%d", tmp->tm_hour);
			fprintf(stderr, " min=%d", tmp->tm_min);
			fprintf(stderr, " sec=%d", tmp->tm_sec);
			fprintf(stderr, " isdst=%d", tmp->tm_isdst);
			fprintf(stderr, " -> ");
			fprintf(stderr, tformat(), t);
			fprintf(stderr, "\n");
		}
	}
	return tmp;
}
#endif /* !defined TYPECHECK */

static void
abbrok(abbrp, zone)
const char * const	abbrp;
const char * const	zone;
{
	const char *	cp;
	char *		wp;

	if (warned)
		return;
	cp = abbrp;
	wp = NULL;
	while (isascii((unsigned char) *cp) && isalpha((unsigned char) *cp))
		++cp;
	if (cp - abbrp == 0)
		wp = _("lacks alphabetic at start");
	else if (cp - abbrp < 3)
		wp = _("has fewer than 3 alphabetics");
	else if (cp - abbrp > 6)
		wp = _("has more than 6 alphabetics");
	if (wp == NULL && (*cp == '+' || *cp == '-')) {
		++cp;
		if (isascii((unsigned char) *cp) &&
			isdigit((unsigned char) *cp))
				if (*cp++ == '1' && *cp >= '0' && *cp <= '4')
					++cp;
		if (*cp != '\0')
			wp = _("differs from POSIX standard");
	}
	if (wp == NULL)
		return;
	fflush(stdout);
	fprintf(stderr, _("%s: warning: zone \"%s\" abbreviation \"%s\" %s\n"),
		progname, zone, abbrp, wp);
	warned = TRUE;
}

static void
usage(void)
{
	fprintf(stderr, _("usage: %s [-v] [-c [loyear,]hiyear] zonename ...\n"), progname);
	exit(EXIT_FAILURE);
}

int
main(argc, argv)
int	argc;
char *	argv[];
{
	int		i;
	int		c;
	int		vflag;
	char *		cutarg;
	long		cutloyear = ZDUMP_LO_YEAR;
	long		cuthiyear = ZDUMP_HI_YEAR;
	time_t		cutlotime;
	time_t		cuthitime;
	char **	fakeenv;
	time_t			now;
	time_t			t;
	time_t			newt;
	struct tm		tm;
	struct tm		newtm;
	struct tm *	tmp;
	struct tm *	newtmp;

	cutlotime = 0;
	cuthitime = 0;
	progname = argv[0];
	vflag = 0;
	cutarg = NULL;
	while ((c = getopt(argc, argv, "c:v")) == 'c' || c == 'v')
		if (c == 'v')
			vflag = 1;
		else	cutarg = optarg;
	if ((c != EOF && c != -1) ||
		(optind == argc - 1 && strcmp(argv[optind], "=") == 0)) {
			usage();
	}
	if (vflag) {
		if (cutarg != NULL) {
			long	lo;
			long	hi;
			char	dummy;

			if (sscanf(cutarg, "%ld%c", &hi, &dummy) == 1) {
				cuthiyear = hi;
			} else if (sscanf(cutarg, "%ld,%ld%c",
				&lo, &hi, &dummy) == 2) {
					cutloyear = lo;
					cuthiyear = hi;
			} else {
				fprintf(stderr, _("%s: wild -c argument %s\n"),
					progname, cutarg);
				exit(EXIT_FAILURE);
			}
		}
		setabsolutes();
		cutlotime = yeartot(cutloyear);
		cuthitime = yeartot(cuthiyear);
	}
	time(&now);
	longest = 0;
	for (i = optind; i < argc; ++i)
		if (strlen(argv[i]) > longest)
			longest = strlen(argv[i]);
	{
		int	from;
		int	to;

		for (i = 0; environ[i] != NULL; ++i)
			continue;
		fakeenv = malloc((size_t) ((i + 2) * sizeof *fakeenv));
		if (fakeenv == NULL ||
			(fakeenv[0] = malloc(longest + 4)) == NULL) {
				perror(progname);
				exit(EXIT_FAILURE);
		}
		to = 0;
		strlcpy(fakeenv[to++], "TZ=", longest + 4);
		for (from = 0; environ[from] != NULL; ++from)
			if (strncmp(environ[from], "TZ=", 3) != 0)
				fakeenv[to++] = environ[from];
		fakeenv[to] = NULL;
		environ = fakeenv;
	}
	for (i = optind; i < argc; ++i) {
		static char	buf[MAX_STRING_LENGTH];

		strlcpy(&fakeenv[0][3], argv[i], longest + 1);
		if (!vflag) {
			show(argv[i], now, FALSE);
			continue;
		}
		warned = FALSE;
		t = absolute_min_time;
		show(argv[i], t, TRUE);
		t += SECSPERHOUR * HOURSPERDAY;
		show(argv[i], t, TRUE);
		if (t < cutlotime)
			t = cutlotime;
		tmp = my_localtime(&t);
		if (tmp != NULL) {
			tm = *tmp;
			strlcpy(buf, abbr(&tm), sizeof buf);
		}
		for ( ; ; ) {
			if (t >= cuthitime || t >= cuthitime - SECSPERHOUR * 12)
				break;
			newt = t + SECSPERHOUR * 12;
			newtmp = localtime(&newt);
			if (newtmp != NULL)
				newtm = *newtmp;
			if ((tmp == NULL || newtmp == NULL) ? (tmp != newtmp) :
				(delta(&newtm, &tm) != (newt - t) ||
				newtm.tm_isdst != tm.tm_isdst ||
				strcmp(abbr(&newtm), buf) != 0)) {
					newt = hunt(argv[i], t, newt);
					newtmp = localtime(&newt);
					if (newtmp != NULL) {
						newtm = *newtmp;
						strlcpy(buf, abbr(&newtm),
							sizeof buf);
					}
			}
			t = newt;
			tm = newtm;
			tmp = newtmp;
		}
		t = absolute_max_time;
		t -= SECSPERHOUR * HOURSPERDAY;
		show(argv[i], t, TRUE);
		t += SECSPERHOUR * HOURSPERDAY;
		show(argv[i], t, TRUE);
	}
	if (fflush(stdout) || ferror(stdout)) {
		fprintf(stderr, "%s: ", progname);
		perror(_("Error writing to standard output"));
		exit(EXIT_FAILURE);
	}
	return 0;
}

static void
setabsolutes(void)
{
	/*
	** time_t is signed.  Assume overflow wraps around.
	*/
	time_t t = 0;
	time_t t1 = 1;

	while (t < t1) {
		t = t1;
		t1 = 2 * t1 + 1;
	}

	absolute_max_time = t;
	t = -t;
	absolute_min_time = t - 1;
	if (t < absolute_min_time)
		absolute_min_time = t;
}

static time_t
yeartot(y)
const long	y;
{
	long	myy;
	long	seconds;
	time_t	t;

	myy = EPOCH_YEAR;
	t = 0;
	while (myy != y) {
		if (myy < y) {
			seconds = isleap(myy) ? SECSPERLYEAR : SECSPERNYEAR;
			++myy;
			if (t > absolute_max_time - seconds) {
				t = absolute_max_time;
				break;
			}
			t += seconds;
		} else {
			--myy;
			seconds = isleap(myy) ? SECSPERLYEAR : SECSPERNYEAR;
			if (t < absolute_min_time + seconds) {
				t = absolute_min_time;
				break;
			}
			t -= seconds;
		}
	}
	return t;
}

static time_t
hunt(char *name, time_t lot, time_t hit)
{
	time_t			t;
	long			diff;
	struct tm		lotm;
	struct tm *	lotmp;
	struct tm		tm;
	struct tm *	tmp;
	char			loab[MAX_STRING_LENGTH];

	lotmp = my_localtime(&lot);
	if (lotmp != NULL) {
		lotm = *lotmp;
		strlcpy(loab, abbr(&lotm), sizeof loab);
	}
	for ( ; ; ) {
		diff = (long) (hit - lot);
		if (diff < 2)
			break;
		t = lot;
		t += diff / 2;
		if (t <= lot)
			++t;
		else if (t >= hit)
			--t;
		tmp = my_localtime(&t);
		if (tmp != NULL)
			tm = *tmp;
		if ((lotmp == NULL || tmp == NULL) ? (lotmp == tmp) :
			(delta(&tm, &lotm) == (t - lot) &&
			tm.tm_isdst == lotm.tm_isdst &&
			strcmp(abbr(&tm), loab) == 0)) {
				lot = t;
				lotm = tm;
				lotmp = tmp;
		} else	hit = t;
	}
	show(name, lot, TRUE);
	show(name, hit, TRUE);
	return hit;
}

/*
** Thanks to Paul Eggert for logic used in delta.
*/

static long
delta(newp, oldp)
struct tm *	newp;
struct tm *	oldp;
{
	long	result;
	int	tmy;

	if (newp->tm_year < oldp->tm_year)
		return -delta(oldp, newp);
	result = 0;
	for (tmy = oldp->tm_year; tmy < newp->tm_year; ++tmy)
		result += DAYSPERNYEAR + isleap_sum(tmy, TM_YEAR_BASE);
	result += newp->tm_yday - oldp->tm_yday;
	result *= HOURSPERDAY;
	result += newp->tm_hour - oldp->tm_hour;
	result *= MINSPERHOUR;
	result += newp->tm_min - oldp->tm_min;
	result *= SECSPERMIN;
	result += newp->tm_sec - oldp->tm_sec;
	return result;
}

static void
show(char *zone, time_t t, int v)
{
	struct tm *	tmp;

	printf("%-*s  ", (int) longest, zone);
	if (v) {
		tmp = gmtime(&t);
		if (tmp == NULL) {
			printf(tformat(), t);
		} else {
			dumptime(tmp);
			printf(" UTC");
		}
		printf(" = ");
	}
	tmp = my_localtime(&t);
	dumptime(tmp);
	if (tmp != NULL) {
		if (*abbr(tmp) != '\0')
			printf(" %s", abbr(tmp));
		if (v) {
			printf(" isdst=%d", tmp->tm_isdst);
#ifdef TM_GMTOFF
			printf(" gmtoff=%ld", tmp->TM_GMTOFF);
#endif /* defined TM_GMTOFF */
		}
	}
	printf("\n");
	if (tmp != NULL && *abbr(tmp) != '\0')
		abbrok(abbr(tmp), zone);
}

static char *
abbr(tmp)
struct tm *	tmp;
{
	char *	result;
	static char	nada;

	if (tmp->tm_isdst != 0 && tmp->tm_isdst != 1)
		return &nada;
	result = tzname[tmp->tm_isdst];
	return (result == NULL) ? &nada : result;
}

/*
** The code below can fail on certain theoretical systems;
** it works on all known real-world systems as of 2004-12-30.
*/

static const char *
tformat(void)
{
	return "%lld";
}

static void
dumptime(timeptr)
const struct tm *	timeptr;
{
	static const char	wday_name[][3] = {
		"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
	};
	static const char	mon_name[][3] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	const char *	wn;
	const char *	mn;
	int		lead;
	int		trail;

	if (timeptr == NULL) {
		printf("NULL");
		return;
	}
	/*
	** The packaged versions of localtime and gmtime never put out-of-range
	** values in tm_wday or tm_mon, but since this code might be compiled
	** with other (perhaps experimental) versions, paranoia is in order.
	*/
	if (timeptr->tm_wday < 0 || timeptr->tm_wday >=
		(int) (sizeof wday_name / sizeof wday_name[0]))
			wn = "???";
	else		wn = wday_name[timeptr->tm_wday];
	if (timeptr->tm_mon < 0 || timeptr->tm_mon >=
		(int) (sizeof mon_name / sizeof mon_name[0]))
			mn = "???";
	else		mn = mon_name[timeptr->tm_mon];
	printf("%.3s %.3s%3d %.2d:%.2d:%.2d ",
		wn, mn,
		timeptr->tm_mday, timeptr->tm_hour,
		timeptr->tm_min, timeptr->tm_sec);
#define DIVISOR	10
	trail = timeptr->tm_year % DIVISOR + TM_YEAR_BASE % DIVISOR;
	lead = timeptr->tm_year / DIVISOR + TM_YEAR_BASE / DIVISOR +
		trail / DIVISOR;
	trail %= DIVISOR;
	if (trail < 0 && lead > 0) {
		trail += DIVISOR;
		--lead;
	} else if (lead < 0 && trail > 0) {
		trail -= DIVISOR;
		++lead;
	}
	if (lead == 0)
		printf("%d", trail);
	else
		printf("%d%d", lead, ((trail < 0) ? -trail : trail));
}
