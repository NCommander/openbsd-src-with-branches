/*	$OpenBSD: fdisk.c,v 1.8 1996/09/27 15:36:09 deraadt Exp $	*/
/*	$NetBSD: fdisk.c,v 1.11 1995/10/04 23:11:19 ghudson Exp $	*/

/*
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: fdisk.c,v 1.8 1996/09/27 15:36:09 deraadt Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#define _PATH_MBR	"/usr/mdec/sdboot"

/*
 * 14-Dec-89  Robert Baron (rvb) at Carnegie-Mellon University
 *	Copyright (c) 1989	Robert. V. Baron
 *	Created.
 */

char *disk;
char *mbrname = _PATH_MBR;

struct	disklabel disklabel;		/* disk parameters */
int	cylinders;
int	sectors;
int	heads;
int	cylindersectors;
int	disksectors;

struct mboot {
	u_int8_t	padding[2];	/* force the longs to be long alligned */
	u_int8_t	bootinst[DOSPARTOFF];
	struct		dos_partition parts[4];
	u_int16_t	signature;
} mboot;

#define ACTIVE		0x80
#define BOOT_MAGIC	0xAA55

int	dos_cylinders;
int	dos_heads;
int	dos_sectors;
int	dos_cylindersectors;

#define	DOSSECT(s,c)	(((s) & 0x3f) | (((c) >> 2) & 0xc0))
#define	DOSCYL(c)	((c) & 0xff)
int	partition = -1;

int	a_flag;		/* set active partition */
int	i_flag;		/* replace partition data */
int	u_flag;		/* update partition data */
int	m_flag;		/* give me a brand new mbr */

struct part_type {
	int	type;
	char	*name;
} part_types[] = {
	{ 0x00, "unused"},
	{ 0x01, "Primary DOS with 12 bit FAT"},
	{ 0x02, "XENIX / filesystem"},
	{ 0x03, "XENIX /usr filesystem"},
	{ 0x04, "Primary DOS with 16 bit FAT"},
	{ 0x05, "Extended DOS"},
	{ 0x06, "Primary 'big' DOS (> 32MB)"},
	{ 0x07, "OS/2 HPFS, QNX or Advanced UNIX"},
	{ 0x08, "AIX filesystem"},
	{ 0x09, "AIX boot partition or Coherent"},
	{ 0x0A, "OS/2 Boot Manager or OPUS"},
	{ 0x10, "OPUS"},
	{ 0x40, "VENIX 286"},
	{ 0x50, "DM"},
	{ 0x51, "DM"},
	{ 0x52, "CP/M or Microport SysV/AT"},
	{ 0x56, "GB"},
	{ 0x61, "Speed"},
	{ 0x63, "ISC, System V/386, GNU HURD or Mach"},
	{ 0x64, "Novell Netware 2.xx"},
	{ 0x65, "Novell Netware 3.xx"},
	{ 0x75, "PCIX"},
	{ 0x80, "Minix 1.1 ... 1.4a"},
	{ 0x81, "Minix 1.4b ... 1.5.10"},
	{ 0x82, "Linux swap"},
	{ 0x83, "Linux filesystem"},
	{ 0x93, "Amoeba filesystem"},
	{ 0x94, "Amoeba bad block table"},
	{ 0xA5, "386BSD/FreeBSD/NetBSD"},
	{ 0xA6, "OpenBSD"},
	{ 0xB7, "BSDI BSD/386 filesystem"},
	{ 0xB8, "BSDI BSD/386 swap"},
	{ 0xDB, "Concurrent CPM or C.DOS or CTOS"},
	{ 0xE1, "Speed"},
	{ 0xE3, "Speed"},
	{ 0xE4, "Speed"},
	{ 0xF1, "Speed"},
	{ 0xF2, "DOS 3.3+ Secondary"},
	{ 0xF4, "Speed"},
	{ 0xFF, "BBT (Bad Blocks Table)"},
};

void	usage __P((void));
void	print_s0 __P((int));
void	print_part __P((int));
void	init_sector0 __P((int));
void	intuit_translated_geometry __P((void));
int	try_heads __P((quad_t, quad_t, quad_t, quad_t, quad_t, quad_t, quad_t,
		       quad_t));
int	try_sectors __P((quad_t, quad_t, quad_t, quad_t, quad_t));
void	change_part __P((int));
void	print_params __P((void));
void	change_active __P((int));
void	get_params_to_use __P((void));
void	dos __P((int, unsigned char *, unsigned char *, unsigned char *));
int	open_disk __P((int));
int	read_disk __P((int, void *));
int	write_disk __P((int, void *));
int	get_params __P((void));
int	read_s0 __P((void));
int	write_s0 __P((void));
int	yesno __P((char *));
void	decimal __P((char *, int *));
int	type_match __P((const void *, const void *));
char	*get_type __P((int));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch, part;

	while ((ch = getopt(argc, argv, "0123aiumf:")) != -1)
		switch (ch) {
		case '0':
			partition = 0;
			break;
		case '1':
			partition = 1;
			break;
		case '2':
			partition = 2;
			break;
		case '3':
			partition = 3;
			break;
		case 'a':
			a_flag = 1;
			break;
		case 'i':
			i_flag = 1;
		case 'u':
			u_flag = 1;
			break;
		case 'm':
			m_flag = 1;
			i_flag = 1;
			u_flag = 1;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage();

	disk = argv[0];

	if (open_disk(a_flag || i_flag || u_flag) < 0)
		exit(1);

	printf("Using device %s:\n", disk);
	if (m_flag || read_s0())
		init_sector0(0);

	intuit_translated_geometry();

	if (u_flag)
		get_params_to_use();
	else
		print_params();

	printf("WARNING: BIOS sector numbers start at 1 (not 0)\n");
	if (partition == -1) {
		for (part = 0; part < NDOSPART; part++)
			change_part(part);
	} else
		change_part(partition);

	if (u_flag || a_flag)
		change_active(partition);

	if (u_flag || a_flag) {
		printf("\nWe haven't changed the partition table yet.  ");
		printf("This is your last chance.\n");
		print_s0(-1);
		if (yesno("Should we write new partition table?"))
			write_s0();
	}

	exit(0);
}

void
usage()
{
	fprintf(stderr, "usage: fdisk [-aium] [-0123] [-f mbrboot] disk\n");
	fprintf(stderr, "-a: change active, -i: initialize, -u: update\n");
	fprintf(stderr, "-m: replace MBR bootblock, -0123: select partition\n");
	fprintf(stderr, "`disk' may be of the forms: sd0 or /dev/rsd0c.\n");
	exit(1);
}

void
print_s0(which)
	int which;
{
	int part;

	print_params();
	printf("Information from DOS bootblock is:\n");
	if (which == -1) {
		for (part = 0; part < NDOSPART; part++)
			print_part(part);
	} else
		print_part(which);
}

static struct dos_partition mtpart = { 0 };

void
print_part(part)
	int part;
{
	struct dos_partition *partp;

	printf("MBR Partition %d: ", part);
	partp = &mboot.parts[part];
	if (!memcmp(partp, &mtpart, sizeof(struct dos_partition))) {
		printf("<UNUSED>\n");
		return;
	}
	printf("sysid %d=0x%02x (%s)\n", partp->dp_typ, partp->dp_typ,
	    get_type(partp->dp_typ));
	printf("    start %d, size %d (%d MB), flag 0x%02x\n",
	    partp->dp_start, partp->dp_size,
	    partp->dp_size * 512 / (1024 * 1024), partp->dp_flag);
	printf("    beg: cylinder %4d, head %3d, sector %2d\n",
	    DPCYL(partp->dp_scyl, partp->dp_ssect),
	    partp->dp_shd, DPSECT(partp->dp_ssect));
	printf("    end: cylinder %4d, head %3d, sector %2d\n",
	    DPCYL(partp->dp_ecyl, partp->dp_esect),
	    partp->dp_ehd, DPSECT(partp->dp_esect));
}

void
init_sector0(start)
	int start;
{
	struct dos_partition *partp;
	FILE *f;

	f = fopen(mbrname, "r");
	if (!f)
		err(1, "cannot open %s", mbrname);

	if (fread(mboot.bootinst, sizeof mboot.bootinst, 1, f) != 1)
		err(1, "reading %s", mbrname);
	fclose(f);

	mboot.signature = BOOT_MAGIC;

	partp = &mboot.parts[3];
	partp->dp_typ = DOSPTYP_OPENBSD;
	partp->dp_flag = ACTIVE;
	partp->dp_start = start;
	partp->dp_size = disksectors - start;

	dos(partp->dp_start,
	    &partp->dp_scyl, &partp->dp_shd, &partp->dp_ssect);
	dos(partp->dp_start + partp->dp_size - 1,
	    &partp->dp_ecyl, &partp->dp_ehd, &partp->dp_esect);
}

/*
 * Prerequisite: the disklabel parameters and master boot record must
 *		 have been read (i.e. dos_* and mboot are meaningful).
 * Specification: modifies dos_cylinders, dos_heads, dos_sectors, and
 *		  dos_cylindersectors to be consistent with what the
 *		  partition table is using, if we can find a geometry
 *		  which is consistent with all partition table entries.
 *		  We may get the number of cylinders slightly wrong (in
 *		  the conservative direction).  The idea is to be able
 *		  to create a NetBSD partition on a disk we don't know
 *		  the translated geometry of.
 * This whole routine should be replaced with a kernel interface to get
 * the BIOS geometry (which in turn requires modifications to the i386
 * boot loader to pass in the BIOS geometry for each disk).
 */
void
intuit_translated_geometry()
{
	int cylinders = -1, heads = -1, sectors = -1, i, j;
	int c1, h1, s1, c2, h2, s2;
	long a1, a2;
	quad_t num, denom;

	/* Try to deduce the number of heads from two different mappings. */
	for (i = 0; i < NDOSPART * 2; i++) {
		if (get_mapping(i, &c1, &h1, &s1, &a1) < 0)
			continue;
		for (j = 0; j < 8; j++) {
			if (get_mapping(j, &c2, &h2, &s2, &a2) < 0)
				continue;
			num = (quad_t)h1*(a2-s2) - h2*(a1-s1);
			denom = (quad_t)c2*(a1-s1) - c1*(a2-s2);
			if (denom != 0 && num % denom == 0) {
				heads = num / denom;
				break;
			}
		}
		if (heads != -1)	
			break;
	}

	if (heads == -1)
		return;

	/* Now figure out the number of sectors from a single mapping. */
	for (i = 0; i < NDOSPART * 2; i++) {
		if (get_mapping(i, &c1, &h1, &s1, &a1) < 0)
			continue;
		num = a1 - s1;
		denom = c1 * heads + h1;
		if (denom != 0 && num % denom == 0) {
			sectors = num / denom;
			break;
		}
	}

	if (sectors == -1)
		return;

	/* Estimate the number of cylinders. */
	cylinders = dos_cylinders * dos_cylindersectors / heads / sectors;

	/* Now verify consistency with each of the partition table entries.
	 * Be willing to shove cylinders up a little bit to make things work,
	 * but translation mismatches are fatal. */
	for (i = 0; i < NDOSPART * 2; i++) {
		if (get_mapping(i, &c1, &h1, &s1, &a1) < 0)
			continue;
		if (sectors * (c1 * heads + h1) + s1 != a1)
			return;
		if (c1 >= cylinders)
			cylinders = c1 + 1;
	}

	/* Everything checks out.  Reset the geometry to use for further
	 * calculations. */
	dos_cylinders = cylinders;
	dos_heads = heads;
	dos_sectors = sectors;
	dos_cylindersectors = heads * sectors;
}

/*
 * For the purposes of intuit_translated_geometry(), treat the partition
 * table as a list of eight mapping between (cylinder, head, sector)
 * triplets and absolute sectors.  Get the relevant geometry triplet and
 * absolute sectors for a given entry, or return -1 if it isn't present.
 * Note: for simplicity, the returned sector is 0-based.
 */
int
get_mapping(i, cylinder, head, sector, absolute)
	int i, *cylinder, *head, *sector;
	long *absolute;
{
	struct dos_partition *part = &mboot.parts[i / 2];

	if (part->dp_typ == 0)
		return -1;
	if (i % 2 == 0) {
		*cylinder = DPCYL(part->dp_scyl, part->dp_ssect);
		*head = part->dp_shd;
		*sector = DPSECT(part->dp_ssect) - 1;
		*absolute = part->dp_start;
	} else {
		*cylinder = DPCYL(part->dp_ecyl, part->dp_esect);
		*head = part->dp_ehd;
		*sector = DPSECT(part->dp_esect) - 1;
		*absolute = part->dp_start + part->dp_size - 1;
	}
	return 0;
}

void
change_part(part)
	int part;
{
	struct dos_partition *partp = &mboot.parts[part];
	int sysid, start, size;

	print_part(part);
	if (!u_flag || !yesno("Do you want to change it?"))
		return;

	if (i_flag) {
		memset(partp, 0, sizeof(*partp));
		if (part == 3) {
			init_sector0(0);
			printf("\nThe static data for the DOS partition 3 has been reinitialized to:\n");
			print_part(part);
		}
	}

	do {
		sysid = partp->dp_typ,
		start = partp->dp_start,
		size = partp->dp_size;
		decimal("sysid", &sysid);
		decimal("start", &start);
		decimal("size", &size);
		partp->dp_typ = sysid;
		partp->dp_start = start;
		partp->dp_size = size;

		if (yesno("Explicitly specify beg/end address?")) {
			int tsector, tcylinder, thead;

			tcylinder = DPCYL(partp->dp_scyl, partp->dp_ssect);
			thead = partp->dp_shd;
			tsector = DPSECT(partp->dp_ssect);
			decimal("beginning cylinder", &tcylinder);
			decimal("beginning head", &thead);
			decimal("beginning sector", &tsector);
			partp->dp_scyl = DOSCYL(tcylinder);
			partp->dp_shd = thead;
			partp->dp_ssect = DOSSECT(tsector, tcylinder);

			tcylinder = DPCYL(partp->dp_ecyl, partp->dp_esect);
			thead = partp->dp_ehd;
			tsector = DPSECT(partp->dp_esect);
			decimal("ending cylinder", &tcylinder);
			decimal("ending head", &thead);
			decimal("ending sector", &tsector);
			partp->dp_ecyl = DOSCYL(tcylinder);
			partp->dp_ehd = thead;
			partp->dp_esect = DOSSECT(tsector, tcylinder);
		} else {
			dos(partp->dp_start,
			    &partp->dp_scyl, &partp->dp_shd, &partp->dp_ssect);
			dos(partp->dp_start + partp->dp_size - 1,
			    &partp->dp_ecyl, &partp->dp_ehd, &partp->dp_esect);
		}

		print_part(part);
	} while (!yesno("Is this entry okay?"));
}

void
print_params()
{

	printf("Parameters extracted from in-core disklabel are:\n");
	printf("    cylinders=%d heads=%d sectors/track=%d\n",
	    cylinders, heads, sectors);
	printf("    sectors/cylinder=%d total=%d\n",
	    cylindersectors, disksectors);
	if (dos_sectors > 63 || dos_cylinders > 1023 || dos_heads > 255)
		printf("Figures below won't work with BIOS for partitions not in cylinder 1\n");
	printf("Parameters to be used for BIOS calculations are:\n");
	printf("    cylinders=%d heads=%d sectors/track=%d\n",
	    dos_cylinders, dos_heads, dos_sectors, dos_cylindersectors);
	printf("    sectors/cylinder=%d\n",
	    dos_cylindersectors);
}

void
change_active(which)
	int which;
{
	struct dos_partition *partp = &mboot.parts[0];
	int part, active = 3;

	if (a_flag && which != -1)
		active = which;
	else {
		for (part = 0; part < NDOSPART; part++)
			if (partp[part].dp_flag & ACTIVE)
				active = part;
	}
	if (yesno("Do you want to change the active partition?")) {
		do {
			decimal("active partition", &active);
		} while (!yesno("Are you happy with this choice?"));
	}
	for (part = 0; part < NDOSPART; part++)
		partp[part].dp_flag &= ~ACTIVE;
	partp[active].dp_flag |= ACTIVE;
}

void
get_params_to_use()
{
	print_params();
	if (yesno("Do you want to change our idea of what BIOS thinks?")) {
		do {
			decimal("BIOS's idea of #cylinders", &dos_cylinders);
			decimal("BIOS's idea of #heads", &dos_heads);
			decimal("BIOS's idea of #sectors", &dos_sectors);
			dos_cylindersectors = dos_heads * dos_sectors;
			print_params();
		} while (!yesno("Are you happy with this choice?"));
	}
}

/*
 * Change real numbers into strange dos numbers
 */
void
dos(sector, cylinderp, headp, sectorp)
	int sector;
	unsigned char *cylinderp, *headp, *sectorp;
{
	int cylinder, head;

	cylinder = sector / dos_cylindersectors;
	sector -= cylinder * dos_cylindersectors;

	head = sector / dos_sectors;
	sector -= head * dos_sectors;

	*cylinderp = DOSCYL(cylinder);
	*headp = head;
	*sectorp = DOSSECT(sector + 1, cylinder);
}

int fd;

int
open_disk(u_flag)
	int u_flag;
{
	struct stat st;

	fd = opendev(disk, (u_flag ? O_RDWR : O_RDONLY), OPENDEV_PART, &disk);
	if (fd == -1) {
		warn("%s", disk);
		return (-1);
	}
	if (fstat(fd, &st) == -1) {
		close(fd);
		warn("%s", disk);
		return (-1);
	}
	if (!S_ISCHR(st.st_mode) && !S_ISREG(st.st_mode)) {
		close(fd);
		warnx("%s is not a character device or regular file", disk);
		return (-1);
	}
	if (get_params() == -1) {
		close(fd);
		return (-1);
	}
	return (0);
}

int
read_disk(sector, buf)
	int sector;
	void *buf;
{
	if (lseek(fd, (off_t)(sector * 512), 0) == -1)
		return (-1);
	return (read(fd, buf, 512));
}

int
write_disk(sector, buf)
	int sector;
	void *buf;
{
	if (lseek(fd, (off_t)(sector * 512), 0) == -1)
		return (-1);
	return (write(fd, buf, 512));
}

int
get_params()
{
	if (ioctl(fd, DIOCGDINFO, &disklabel) == -1) {
		warn("DIOCGDINFO");
		return (-1);
	}

	dos_cylinders = cylinders = disklabel.d_ncylinders;
	dos_heads = heads = disklabel.d_ntracks;
	dos_sectors = sectors = disklabel.d_nsectors;
	dos_cylindersectors = cylindersectors = heads * sectors;
	disksectors = cylinders * heads * sectors;
	return (0);
}

int
read_s0()
{

	if (read_disk(0, mboot.bootinst) == -1) {
		warn("can't read fdisk partition table");
		return (-1);
	}
	if (mboot.signature != BOOT_MAGIC) {
		fprintf(stderr,
		    "warning: invalid fdisk partition table found!\n");
		/* So should we initialize things? */
		return (-1);
	}
	return (0);
}

int
write_s0()
{
	int flag;

	/*
	 * write enable label sector before write (if necessary),
	 * disable after writing.
	 * needed if the disklabel protected area also protects
	 * sector 0. (e.g. empty disk)
	 */
	flag = 1;
	if (ioctl(fd, DIOCWLABEL, &flag) < 0)
		warn("DIOCWLABEL");
	if (write_disk(0, mboot.bootinst) == -1) {
		warn("can't write fdisk partition table");
		return -1;
	}
	flag = 0;
	if (ioctl(fd, DIOCWLABEL, &flag) < 0)
		warn("DIOCWLABEL");
}

int
yesno(str)
	char *str;
{
	int ch, first;

	printf("%s [n] ", str);

	first = ch = getchar();
	while (ch != '\n' && ch != EOF)
		ch = getchar();
	return (first == 'y' || first == 'Y');
}

void
decimal(str, num)
	char *str;
	int *num;
{
	char lbuf[100], *cp;
	int acc = 0;

	while (1) {
		printf("Supply a decimal value for \"%s\" [%d] ", str, *num);

		if (fgets(lbuf, sizeof lbuf, stdin) == NULL)
			errx(1, "eof");
		lbuf[strlen(lbuf)-1] = '\0';

		cp = lbuf;
		cp += strspn(cp, " \t");
		if (*cp == '\0')
			return;
		if (!isdigit(*cp))
			goto bad;
		acc = strtol(lbuf, &cp, 10);

		cp += strspn(cp, " \t");
		if (*cp != '\0')
			goto bad;
		*num = acc;
		return;
bad:
		printf("%s is not a valid decimal number.\n", lbuf);
	}
}

int
type_match(key, item)
	const void *key, *item;
{
	const int *typep = key;
	const struct part_type *ptr = item;

	if (*typep < ptr->type)
		return (-1);
	if (*typep > ptr->type)
		return (1);
	return (0);
}

char *
get_type(type)
	int type;
{
	struct part_type *ptr;

	ptr = bsearch(&type, part_types,
	    sizeof(part_types) / sizeof(struct part_type),
	    sizeof(struct part_type), type_match);
	if (ptr == 0)
		return ("unknown");
	return (ptr->name);
}
