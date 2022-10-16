/*	$OpenBSD$	*/

/*
 * Copyright (c) 2022 Mark Kettenis <kettenis@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/user.h>

#include <uvm/uvm_extern.h>

#include <machine/biosvar.h>
extern paddr_t cr3_reuse_pcid;

#include <dev/efi/efi.h>

#include <dev/clock_subr.h>

extern EFI_MEMORY_DESCRIPTOR *mmap;

struct efi_softc {
	struct device	sc_dev;
	struct pmap	*sc_pm;
	EFI_RUNTIME_SERVICES *sc_rs;
	u_long		sc_psw;
	uint64_t	sc_cr3;

	struct todr_chip_handle sc_todr;
};

int	efi_match(struct device *, void *, void *);
void	efi_attach(struct device *, struct device *, void *);

const struct cfattach efi_ca = {
	sizeof(struct efi_softc), efi_match, efi_attach
};

struct cfdriver efi_cd = {
	NULL, "efi", DV_DULL
};

void	efi_map_runtime(struct efi_softc *);
void	efi_enter(struct efi_softc *);
void	efi_leave(struct efi_softc *);
int	efi_gettime(struct todr_chip_handle *, struct timeval *);
int	efi_settime(struct todr_chip_handle *, struct timeval *);

int
efi_match(struct device *parent, void *match, void *aux)
{
	struct bios_attach_args	*ba = aux;
	struct cfdata *cf = match;

	if (strcmp(ba->ba_name, cf->cf_driver->cd_name) == 0 &&
	    bios_efiinfo->system_table != 0)
		return 1;

	return 0;
}

void
efi_attach(struct device *parent, struct device *self, void *aux)
{
	struct efi_softc *sc = (struct efi_softc *)self;
	struct bios_attach_args *ba = aux;
	uint32_t mmap_desc_ver = bios_efiinfo->mmap_desc_ver;
	uint64_t system_table;
	bus_space_handle_t memh;
	EFI_SYSTEM_TABLE *st;
	EFI_TIME time;
	EFI_STATUS status;
	uint16_t major, minor;
	int i;

	if (mmap_desc_ver != EFI_MEMORY_DESCRIPTOR_VERSION) {
		printf(": unsupported memory descriptor version %d\n",
		    mmap_desc_ver);
		return;
	}

	system_table = bios_efiinfo->system_table;
	KASSERT(system_table);

	if (bus_space_map(ba->ba_memt, system_table, sizeof(EFI_SYSTEM_TABLE),
	    BUS_SPACE_MAP_LINEAR | BUS_SPACE_MAP_CACHEABLE, &memh)) {
		printf(": can't map system table\n");
		return;
	}

	st = bus_space_vaddr(ba->ba_memt, memh);
	sc->sc_rs = st->RuntimeServices;

	major = st->Hdr.Revision >> 16;
	minor = st->Hdr.Revision & 0xffff;
	printf(": UEFI %d.%d", major, minor / 10);
	if (minor % 10)
		printf(".%d", minor % 10);
	printf("\n");

	if ((bios_efiinfo->flags & BEI_64BIT) == 0)
		return;

	efi_map_runtime(sc);

	/*
	 * Activate our pmap such that we can access the
	 * FirmwareVendor and ConfigurationTable fields.
	 */
	efi_enter(sc);
	if (st->FirmwareVendor) {
		printf("%s: ", sc->sc_dev.dv_xname);
		for (i = 0; st->FirmwareVendor[i]; i++)
			printf("%c", st->FirmwareVendor[i]);
		printf(" rev 0x%x\n", st->FirmwareRevision);
	}
	efi_leave(sc);

	efi_enter(sc);
	status = sc->sc_rs->GetTime(&time, NULL);
	efi_leave(sc);
	if (status != EFI_SUCCESS)
		return;

	/*
	 * EDK II implementations provide an implementation of
	 * GetTime() that returns a fixed compiled-in time on hardware
	 * without a (supported) RTC.  So only use this interface as a
	 * last resort.
	 */
	sc->sc_todr.cookie = sc;
	sc->sc_todr.todr_gettime = efi_gettime;
	sc->sc_todr.todr_settime = efi_settime;
	sc->sc_todr.todr_quality = -1000;
	todr_attach(&sc->sc_todr);
}

void
efi_map_runtime(struct efi_softc *sc)
{
	uint32_t mmap_size = bios_efiinfo->mmap_size;
	uint32_t mmap_desc_size = bios_efiinfo->mmap_desc_size;
	EFI_MEMORY_DESCRIPTOR *desc;
	int i;

	/*
	 * We don't really want some random executable non-OpenBSD
	 * code lying around in kernel space.  So create a separate
	 * pmap and only activate it when we call runtime services.
	 */
	sc->sc_pm = pmap_create();

	desc = mmap;
	for (i = 0; i < mmap_size / mmap_desc_size; i++) {
		if (desc->Attribute & EFI_MEMORY_RUNTIME) {
			vaddr_t va = desc->VirtualStart;
			paddr_t pa = desc->PhysicalStart;
			int npages = desc->NumberOfPages;
			vm_prot_t prot = PROT_READ | PROT_WRITE;

#ifdef EFI_DEBUG
			printf("type 0x%x pa 0x%llx va 0x%llx pages 0x%llx attr 0x%llx\n",
			    desc->Type, desc->PhysicalStart,
			    desc->VirtualStart, desc->NumberOfPages,
			    desc->Attribute);
#endif

			/*
			 * If the virtual address is still zero, use
			 * an identity mapping.
			 */
			if (va == 0)
				va = pa;

			/*
			 * Normal memory is expected to be "write
			 * back" cacheable.  Everything else is mapped
			 * as device memory.
			 */
			if ((desc->Attribute & EFI_MEMORY_WB) == 0)
				pa |= PMAP_NOCACHE;

			/*
			 * Only make pages marked as runtime service code
			 * executable.  This violates the standard but it
			 * seems we can get away with it.
			 */
			if (desc->Type == EfiRuntimeServicesCode)
				prot |= PROT_EXEC;

			if (desc->Attribute & EFI_MEMORY_RP)
				prot &= ~PROT_READ;
			if (desc->Attribute & EFI_MEMORY_XP)
				prot &= ~PROT_EXEC;
			if (desc->Attribute & EFI_MEMORY_RO)
				prot &= ~PROT_WRITE;

			while (npages--) {
				pmap_enter(sc->sc_pm, va, pa, prot,
				   prot | PMAP_WIRED | PMAP_EFI);
				va += PAGE_SIZE;
				pa += PAGE_SIZE;
			}
		}

		desc = NextMemoryDescriptor(desc, mmap_desc_size);
	}
}

void
efi_enter(struct efi_softc *sc)
{
	sc->sc_psw = intr_disable();
	sc->sc_cr3 = rcr3() | cr3_reuse_pcid;
	lcr3(sc->sc_pm->pm_pdirpa | (pmap_use_pcid ? PCID_EFI : 0));

	fpu_kernel_enter();
}

void
efi_leave(struct efi_softc *sc)
{
	fpu_kernel_exit();

	lcr3(sc->sc_cr3);
	intr_restore(sc->sc_psw);
}

int
efi_gettime(struct todr_chip_handle *handle, struct timeval *tv)
{
	struct efi_softc *sc = handle->cookie;
	struct clock_ymdhms dt;
	EFI_TIME time;
	EFI_STATUS status;

	efi_enter(sc);
	status = sc->sc_rs->GetTime(&time, NULL);
	efi_leave(sc);
	if (status != EFI_SUCCESS)
		return EIO;

	dt.dt_year = time.Year;
	dt.dt_mon = time.Month;
	dt.dt_day = time.Day;
	dt.dt_hour = time.Hour;
	dt.dt_min = time.Minute;
	dt.dt_sec = time.Second;

	if (dt.dt_sec > 59 || dt.dt_min > 59 || dt.dt_hour > 23 ||
	    dt.dt_day > 31 || dt.dt_day == 0 ||
	    dt.dt_mon > 12 || dt.dt_mon == 0 ||
	    dt.dt_year < POSIX_BASE_YEAR)
		return EINVAL;

	tv->tv_sec = clock_ymdhms_to_secs(&dt);
	tv->tv_usec = 0;
	return 0;
}

int
efi_settime(struct todr_chip_handle *handle, struct timeval *tv)
{
	struct efi_softc *sc = handle->cookie;
	struct clock_ymdhms dt;
	EFI_TIME time;
	EFI_STATUS status;

	clock_secs_to_ymdhms(tv->tv_sec, &dt);

	time.Year = dt.dt_year;
	time.Month = dt.dt_mon;
	time.Day = dt.dt_day;
	time.Hour = dt.dt_hour;
	time.Minute = dt.dt_min;
	time.Second = dt.dt_sec;
	time.Nanosecond = 0;
	time.TimeZone = 0;
	time.Daylight = 0;

	efi_enter(sc);
	status = sc->sc_rs->SetTime(&time);
	efi_leave(sc);
	if (status != EFI_SUCCESS)
		return EIO;
	return 0;
}
