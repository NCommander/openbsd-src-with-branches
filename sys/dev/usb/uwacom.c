/*	$OpenBSD: uwacom.c,v 1.1 2016/09/12 08:12:06 mpi Exp $	*/

/*
 * Copyright (c) 2016 Frank Groeneveld <frank@frankgroeneveld.nl>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* Driver for USB Wacom tablets */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/uhidev.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>

#include <dev/hid/hidmsvar.h>

struct uwacom_softc {
	struct uhidev		sc_hdev;
	struct hidms		sc_ms;
	struct hid_location	sc_loc_tip_press;
};

struct cfdriver uwacom_cd = {
	NULL, "uwacom", DV_DULL
};


const struct usb_devno uwacom_devs[] = {
	{ USB_VENDOR_WACOM, USB_PRODUCT_WACOM_INTUOS_DRAW }
};

int	uwacom_match(struct device *, void *, void *);
void	uwacom_attach(struct device *, struct device *, void *);
int	uwacom_detach(struct device *, int);
void	uwacom_intr(struct uhidev *, void *, u_int);
int	uwacom_enable(void *);
void	uwacom_disable(void *);
int	uwacom_ioctl(void *, u_long, caddr_t, int, struct proc *);

const struct cfattach uwacom_ca = {
	sizeof(struct uwacom_softc), uwacom_match, uwacom_attach, uwacom_detach
};

const struct wsmouse_accessops uwacom_accessops = {
	uwacom_enable,
	uwacom_ioctl,
	uwacom_disable,
};

int
uwacom_match(struct device *parent, void *match, void *aux)
{
	struct uhidev_attach_arg *uha = aux;
	int size;
	void *desc;

	if (usb_lookup(uwacom_devs, uha->uaa->vendor,
	    uha->uaa->product) == NULL)
		return (UMATCH_NONE);

	uhidev_get_report_desc(uha->parent, &desc, &size);

	if (!hid_locate(desc, size, HID_USAGE2(HUP_WACOM, HUG_POINTER),
	    uha->reportid, hid_input, NULL, NULL))
		return (UMATCH_NONE);

	return (UMATCH_IFACECLASS);
}

void
uwacom_attach(struct device *parent, struct device *self, void *aux)
{
	struct uwacom_softc *sc = (struct uwacom_softc *)self;
	struct hidms *ms = &sc->sc_ms;
	struct uhidev_attach_arg *uha = (struct uhidev_attach_arg *)aux;
	struct usb_attach_arg *uaa = uha->uaa;
	int size, repid;
	void *desc;

	sc->sc_hdev.sc_intr = uwacom_intr;
	sc->sc_hdev.sc_parent = uha->parent;
	sc->sc_hdev.sc_udev = uaa->device;
	sc->sc_hdev.sc_report_id = uha->reportid;

	usbd_set_idle(uha->parent->sc_udev, uha->parent->sc_ifaceno, 0, 0);

	uhidev_get_report_desc(uha->parent, &desc, &size);
	repid = uha->reportid;
	sc->sc_hdev.sc_isize = hid_report_size(desc, size, hid_input, repid);
	sc->sc_hdev.sc_osize = hid_report_size(desc, size, hid_output, repid);
	sc->sc_hdev.sc_fsize = hid_report_size(desc, size, hid_feature, repid);

	ms->sc_device = self;
	ms->sc_rawmode = 1;
	ms->sc_flags = HIDMS_ABSX | HIDMS_ABSY;
	ms->sc_num_buttons = 3;
	ms->sc_loc_x.pos = 8;
	ms->sc_loc_x.size = 16;
	ms->sc_loc_y.pos = 24;
	ms->sc_loc_y.size = 16;

	ms->sc_tsscale.minx = 0;
	ms->sc_tsscale.maxx = 7600;
	ms->sc_tsscale.miny = 0;
	ms->sc_tsscale.maxy = 4750;

	ms->sc_loc_btn[0].pos = 0;
	ms->sc_loc_btn[0].size = 1;
	ms->sc_loc_btn[1].pos = 1;
	ms->sc_loc_btn[1].size = 1;
	ms->sc_loc_btn[2].pos = 2;
	ms->sc_loc_btn[2].size = 1;

	sc->sc_loc_tip_press.pos = 43;
	sc->sc_loc_tip_press.size = 8;

	hidms_attach(ms, &uwacom_accessops);
}

int
uwacom_detach(struct device *self, int flags)
{
	struct uwacom_softc *sc = (struct uwacom_softc *)self;
	struct hidms *ms = &sc->sc_ms;

	return hidms_detach(ms, flags);
}

void
uwacom_intr(struct uhidev *addr, void *buf, u_int len)
{
	struct uwacom_softc *sc = (struct uwacom_softc *)addr;
	struct hidms *ms = &sc->sc_ms;
	u_int32_t buttons = 0;
	uint8_t *data = (uint8_t *)buf;
	int i, x, y, pressure;

	if (ms->sc_enabled == 0)
		return;

	/* ignore proximity, it will cause invalid button 2 events */
	if ((data[0] & 0xf0) == 0xc0)
		return;

	x = be16toh(hid_get_data(data, len, &ms->sc_loc_x));
	y = be16toh(hid_get_data(data, len, &ms->sc_loc_y));
	pressure = hid_get_data(data, len, &sc->sc_loc_tip_press);

	for (i = 0; i < ms->sc_num_buttons; i++)
		if (hid_get_data(data, len, &ms->sc_loc_btn[i]))
			buttons |= (1 << i);

	/* button 0 reporting is flaky, use tip pressure for it */
	if (pressure > 10)
		buttons |= 1;
	else
		buttons &= ~1;

	if (x != 0 || y != 0 || buttons != ms->sc_buttons) {
		wsmouse_position(ms->sc_wsmousedev, x, y);
		wsmouse_buttons(ms->sc_wsmousedev, buttons);
		wsmouse_input_sync(ms->sc_wsmousedev);
	}
}

int
uwacom_enable(void *v)
{
	struct uwacom_softc *sc = v;
	struct hidms *ms = &sc->sc_ms;
	int rv;

	if (usbd_is_dying(sc->sc_hdev.sc_udev))
		return EIO;

	if ((rv = hidms_enable(ms)) != 0)
		return rv;

	return uhidev_open(&sc->sc_hdev);
}

void
uwacom_disable(void *v)
{
	struct uwacom_softc *sc = v;
	struct hidms *ms = &sc->sc_ms;

	hidms_disable(ms);
	uhidev_close(&sc->sc_hdev);
}

int
uwacom_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct uwacom_softc *sc = v;
	struct hidms *ms = &sc->sc_ms;
	int rc;

	switch (cmd) {
	case WSMOUSEIO_GTYPE:
		*(u_int *)data = WSMOUSE_TYPE_TPANEL;
		return 0;
	}

	rc = uhidev_ioctl(&sc->sc_hdev, cmd, data, flag, p);
	if (rc != -1)
		return rc;

	return hidms_ioctl(ms, cmd, data, flag, p);
}

