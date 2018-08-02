/* $OpenBSD: ssdfb.c,v 1.3 2018/08/01 12:34:36 patrick Exp $ */
/*
 * Copyright (c) 2018 Patrick Wildt <patrick@blueri.se>
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
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/stdint.h>
#include <sys/timeout.h>
#include <sys/task.h>

#include <dev/i2c/i2cvar.h>
#include <dev/spi/spivar.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_gpio.h>
#include <dev/ofw/ofw_pinctrl.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>

#define SSDFB_SET_LOWER_COLUMN_START_ADDRESS	0x00
#define SSDFB_SET_HIGHER_COLUMN_START_ADDRESS	0x10
#define SSDFB_SET_MEMORY_ADDRESSING_MODE	0x20
#define SSDFB_SET_COLUMN_RANGE			0x21
#define SSDFB_SET_PAGE_RANGE			0x22
#define SSDFB_SET_START_LINE			0x40
#define SSDFB_SET_CONTRAST_CONTROL		0x81
#define SSDFB_SET_COLUMN_DIRECTION_REVERSE	0xa1
#define SSDFB_SET_MULTIPLEX_RATIO		0xa8
#define SSDFB_SET_COM_OUTPUT_DIRECTION		0xc0
#define SSDFB_ENTIRE_DISPLAY_ON			0xa4
#define SSDFB_SET_DISPLAY_MODE_NORMAL		0xa6
#define SSDFB_SET_DISPLAY_MODE_INVERS		0xa7
#define SSDFB_SET_DISPLAY_OFF			0xae
#define SSDFB_SET_DISPLAY_ON			0xaf
#define SSDFB_SET_DISPLAY_OFFSET		0xd3
#define SSDFB_SET_DISPLAY_CLOCK_DIVIDE_RATIO	0xd5
#define SSDFB_SET_PRE_CHARGE_PERIOD		0xd9
#define SSDFB_SET_COM_PINS_HARD_CONF		0xda
#define SSDFB_SET_VCOM_DESELECT_LEVEL		0xdb
#define SSDFB_SET_PAGE_START_ADDRESS		0xb0

#define SSDFB_I2C_COMMAND			0x00
#define SSDFB_I2C_DATA				0x40

#define SSDFB_WIDTH	128
#define SSDFB_HEIGHT	64

struct ssdfb_softc {
	struct device		 sc_dev;
	int			 sc_node;

	uint8_t			*sc_fb;
	size_t			 sc_fbsize;
	struct rasops_info	 sc_rinfo;

	uint8_t			 sc_column_range[2];
	uint8_t			 sc_page_range[2];

	struct task		 sc_task;
	struct timeout		 sc_to;

	/* I2C */
	i2c_tag_t		 sc_i2c_tag;
	i2c_addr_t		 sc_i2c_addr;

	/* SPI */
	spi_tag_t		 sc_spi_tag;
	struct spi_config	 sc_spi_conf;
	uint32_t		*sc_gpio;
	size_t			 sc_gpiolen;
	int			 sc_cd;

	void			 (*sc_write_command)(struct ssdfb_softc *,
				   char *, size_t);
	void			 (*sc_write_data)(struct ssdfb_softc *,
				   char *, size_t);

};

int	 ssdfb_i2c_match(struct device *, void *, void *);
void	 ssdfb_i2c_attach(struct device *, struct device *, void *);
int	 ssdfb_i2c_detach(struct device *, int);
void	 ssdfb_i2c_write_command(struct ssdfb_softc *, char *, size_t);
void	 ssdfb_i2c_write_data(struct ssdfb_softc *, char *, size_t);

int	 ssdfb_spi_match(struct device *, void *, void *);
void	 ssdfb_spi_attach(struct device *, struct device *, void *);
int	 ssdfb_spi_detach(struct device *, int);
void	 ssdfb_spi_write_command(struct ssdfb_softc *, char *, size_t);
void	 ssdfb_spi_write_data(struct ssdfb_softc *, char *, size_t);

void	 ssdfb_attach(struct ssdfb_softc *);
int	 ssdfb_detach(struct ssdfb_softc *, int);
void	 ssdfb_write_command(struct ssdfb_softc *, char *, size_t);
void	 ssdfb_write_data(struct ssdfb_softc *, char *, size_t);

void	 ssdfb_init(struct ssdfb_softc *);
void	 ssdfb_update(void *);
void	 ssdfb_timeout(void *);

void	 ssdfb_partial(struct ssdfb_softc *, uint32_t, uint32_t,
	    uint32_t, uint32_t);
void	 ssdfb_set_range(struct ssdfb_softc *, uint8_t, uint8_t,
	    uint8_t, uint8_t);

int	 ssdfb_ioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t	 ssdfb_mmap(void *, off_t, int);
int	 ssdfb_alloc_screen(void *, const struct wsscreen_descr *, void **,
	    int *, int *, long *);
void	 ssdfb_free_screen(void *, void *);
int	 ssdfb_show_screen(void *, void *, int, void (*cb) (void *, int, int),
	    void *);
int	 ssdfb_list_font(void *, struct wsdisplay_font *);
int	 ssdfb_load_font(void *, void *, struct wsdisplay_font *);

struct cfattach ssdfb_i2c_ca = {
	sizeof(struct ssdfb_softc),
	ssdfb_i2c_match,
	ssdfb_i2c_attach,
	ssdfb_i2c_detach,
};

struct cfattach ssdfb_spi_ca = {
	sizeof(struct ssdfb_softc),
	ssdfb_spi_match,
	ssdfb_spi_attach,
	ssdfb_spi_detach,
};

struct cfdriver ssdfb_cd = {
	NULL, "ssdfb", DV_DULL
};

struct wsscreen_descr ssdfb_std_descr = { "std" };
struct wsdisplay_charcell ssdfb_bs[SSDFB_WIDTH * SSDFB_HEIGHT];

const struct wsscreen_descr *ssdfb_descrs[] = {
	&ssdfb_std_descr
};

const struct wsscreen_list ssdfb_screen_list = {
	nitems(ssdfb_descrs), ssdfb_descrs
};

struct wsdisplay_accessops ssdfb_accessops = {
	.ioctl = ssdfb_ioctl,
	.mmap = ssdfb_mmap,
	.alloc_screen = ssdfb_alloc_screen,
	.free_screen = ssdfb_free_screen,
	.show_screen = ssdfb_show_screen,
	.load_font = ssdfb_load_font,
	.list_font = ssdfb_list_font
};

int
ssdfb_i2c_match(struct device *parent, void *match, void *aux)
{
	struct i2c_attach_args *ia = aux;

	if (strcmp(ia->ia_name, "solomon,ssd1309fb-i2c") == 0)
		return 1;

	return 0;
}

void
ssdfb_i2c_attach(struct device *parent, struct device *self, void *aux)
{
	struct ssdfb_softc *sc = (struct ssdfb_softc *)self;
	struct i2c_attach_args *ia = aux;

	sc->sc_i2c_tag = ia->ia_tag;
	sc->sc_i2c_addr = ia->ia_addr;
	sc->sc_node = *(int *)ia->ia_cookie;

	sc->sc_write_command = ssdfb_i2c_write_command;
	sc->sc_write_data = ssdfb_i2c_write_data;

	ssdfb_attach(sc);
}

int
ssdfb_i2c_detach(struct device *self, int flags)
{
	struct ssdfb_softc *sc = (struct ssdfb_softc *)self;
	ssdfb_detach(sc, flags);
	free(sc->sc_gpio, M_DEVBUF, sc->sc_gpiolen);
	return 0;
}

int
ssdfb_spi_match(struct device *parent, void *match, void *aux)
{
	struct spi_attach_args *sa = aux;

	if (strcmp(sa->sa_name, "solomon,ssd1309fb-spi") == 0)
		return 1;

	return 0;
}

void
ssdfb_spi_attach(struct device *parent, struct device *self, void *aux)
{
	struct ssdfb_softc *sc = (struct ssdfb_softc *)self;
	struct spi_attach_args *sa = aux;
	ssize_t len;

	sc->sc_spi_tag = sa->sa_tag;
	sc->sc_node = *(int *)sa->sa_cookie;

	sc->sc_spi_conf.sc_bpw = 8;
	sc->sc_spi_conf.sc_freq = 1000 * 1000;
	sc->sc_spi_conf.sc_cs = OF_getpropint(sc->sc_node, "reg", 0);
	if (OF_getproplen(sc->sc_node, "spi-cpol") == 0)
		sc->sc_spi_conf.sc_flags |= SPI_CONFIG_CPOL;
	if (OF_getproplen(sc->sc_node, "spi-cpha") == 0)
		sc->sc_spi_conf.sc_flags |= SPI_CONFIG_CPHA;
	if (OF_getproplen(sc->sc_node, "spi-cs-high") == 0)
		sc->sc_spi_conf.sc_flags |= SPI_CONFIG_CS_HIGH;

	len = OF_getproplen(sc->sc_node, "cd-gpio");
	if (len <= 0)
		return;

	sc->sc_gpio = malloc(len, M_DEVBUF, M_WAITOK);
	OF_getpropintarray(sc->sc_node, "cd-gpio", sc->sc_gpio, len);
	sc->sc_gpiolen = len;
	gpio_controller_config_pin(sc->sc_gpio, GPIO_CONFIG_OUTPUT);
	gpio_controller_set_pin(sc->sc_gpio, 0);

	sc->sc_write_command = ssdfb_spi_write_command;
	sc->sc_write_data = ssdfb_spi_write_data;

	ssdfb_attach(sc);
}

int
ssdfb_spi_detach(struct device *self, int flags)
{
	struct ssdfb_softc *sc = (struct ssdfb_softc *)self;
	ssdfb_detach(sc, flags);
	free(sc->sc_gpio, M_DEVBUF, sc->sc_gpiolen);
	return 0;
}

void
ssdfb_attach(struct ssdfb_softc *sc)
{
	struct wsemuldisplaydev_attach_args aa;
	struct rasops_info *ri;
	uint32_t *gpio;
	ssize_t len;

	pinctrl_byname(sc->sc_node, "default");

	len = OF_getproplen(sc->sc_node, "reset-gpio");
	if (len > 0) {
		gpio = malloc(len, M_DEVBUF, M_WAITOK);
		OF_getpropintarray(sc->sc_node, "reset-gpio",
		    gpio, len);
		gpio_controller_config_pin(gpio, GPIO_CONFIG_OUTPUT);
		gpio_controller_set_pin(gpio, 1);
		delay(100 * 1000);
		gpio_controller_set_pin(gpio, 0);
		delay(1000 * 1000);
		free(gpio, M_DEVBUF, len);
	}

	sc->sc_fbsize = (SSDFB_WIDTH * SSDFB_HEIGHT) / 8;
	sc->sc_fb = malloc(sc->sc_fbsize, M_DEVBUF, M_WAITOK | M_ZERO);

	ri = &sc->sc_rinfo;
	ri->ri_bits = malloc(sc->sc_fbsize, M_DEVBUF, M_WAITOK | M_ZERO);
	ri->ri_bs = ssdfb_bs;
	ri->ri_flg = RI_CLEAR | RI_VCONS;
	ri->ri_depth = 1;
	ri->ri_width = SSDFB_WIDTH;
	ri->ri_height = SSDFB_HEIGHT;
	ri->ri_stride = ri->ri_width * ri->ri_depth / 8;

	rasops_init(ri, SSDFB_HEIGHT, SSDFB_WIDTH);
	ssdfb_std_descr.ncols = ri->ri_cols;
	ssdfb_std_descr.nrows = ri->ri_rows;
	ssdfb_std_descr.textops = &ri->ri_ops;
	ssdfb_std_descr.fontwidth = ri->ri_font->fontwidth;
	ssdfb_std_descr.fontheight = ri->ri_font->fontheight;
	ssdfb_std_descr.capabilities = ri->ri_caps;

	task_set(&sc->sc_task, ssdfb_update, sc);
	timeout_set(&sc->sc_to, ssdfb_timeout, sc);

	printf(": %dx%d, %dbpp\n", ri->ri_width, ri->ri_height, ri->ri_depth);

	memset(&aa, 0, sizeof(aa));
	aa.console = 0;
	aa.scrdata = &ssdfb_screen_list;
	aa.accessops = &ssdfb_accessops;
	aa.accesscookie = sc;
	aa.defaultscreens = 0;

	config_found_sm(&sc->sc_dev, &aa, wsemuldisplaydevprint,
	    wsemuldisplaydevsubmatch);
	ssdfb_init(sc);
}

int
ssdfb_detach(struct ssdfb_softc *sc, int flags)
{
	struct rasops_info *ri = &sc->sc_rinfo;
	timeout_del(&sc->sc_to);
	task_del(systq, &sc->sc_task);
	free(ri->ri_bits, M_DEVBUF, sc->sc_fbsize);
	free(sc->sc_fb, M_DEVBUF, sc->sc_fbsize);
	return 0;
}

void
ssdfb_init(struct ssdfb_softc *sc)
{
	uint8_t reg[2];

	reg[0] = SSDFB_SET_DISPLAY_OFF;
	ssdfb_write_command(sc, reg, 1);

	reg[0] = SSDFB_SET_MEMORY_ADDRESSING_MODE;
	reg[1] = 0x00; /* Horizontal Addressing Mode */
	ssdfb_write_command(sc, reg, 2);
	reg[0] = SSDFB_SET_PAGE_START_ADDRESS;
	ssdfb_write_command(sc, reg, 1);
	ssdfb_set_range(sc, 0, SSDFB_WIDTH - 1,
	    0, (SSDFB_HEIGHT / 8) - 1);
	reg[0] = SSDFB_SET_DISPLAY_CLOCK_DIVIDE_RATIO;
	reg[1] = 0xa0;
	ssdfb_write_command(sc, reg, 2);
	reg[0] = SSDFB_SET_MULTIPLEX_RATIO;
	reg[1] = 0x3f;
	ssdfb_write_command(sc, reg, 2);
	reg[0] = SSDFB_SET_DISPLAY_OFFSET;
	reg[1] = 0x00;
	ssdfb_write_command(sc, reg, 2);
	reg[0] = SSDFB_SET_START_LINE | 0x00;
	ssdfb_write_command(sc, reg, 1);
	reg[0] = SSDFB_SET_COLUMN_DIRECTION_REVERSE;
	ssdfb_write_command(sc, reg, 1);
	reg[0] = SSDFB_SET_COM_OUTPUT_DIRECTION | 0x08;
	ssdfb_write_command(sc, reg, 1);
	reg[0] = SSDFB_SET_COM_PINS_HARD_CONF;
	reg[1] = 0x12;
	ssdfb_write_command(sc, reg, 2);
	reg[0] = SSDFB_SET_CONTRAST_CONTROL;
	reg[1] = 223;
	ssdfb_write_command(sc, reg, 2);
	reg[0] = SSDFB_SET_PRE_CHARGE_PERIOD;
	reg[1] = 0x82;
	ssdfb_write_command(sc, reg, 2);
	reg[0] = SSDFB_SET_VCOM_DESELECT_LEVEL;
	reg[1] = 0x34;
	ssdfb_write_command(sc, reg, 2);
	reg[0] = SSDFB_ENTIRE_DISPLAY_ON;
	ssdfb_write_command(sc, reg, 1);
	reg[0] = SSDFB_SET_DISPLAY_MODE_NORMAL;
	ssdfb_write_command(sc, reg, 1);

	ssdfb_update(sc);

	reg[0] = SSDFB_SET_DISPLAY_ON;
	ssdfb_write_command(sc, reg, 1);
}

void
ssdfb_set_range(struct ssdfb_softc *sc, uint8_t x1, uint8_t x2,
    uint8_t y1, uint8_t y2)
{
	uint8_t reg[3];

	if (sc->sc_column_range[0] != x1 || sc->sc_column_range[1] != x2) {
		sc->sc_column_range[0] = x1;
		sc->sc_column_range[1] = x2;
		reg[0] = SSDFB_SET_COLUMN_RANGE;
		reg[1] = sc->sc_column_range[0];
		reg[2] = sc->sc_column_range[1];
		ssdfb_write_command(sc, reg, 3);
	}
	if (sc->sc_page_range[0] != y1 || sc->sc_page_range[1] != y2) {
		sc->sc_page_range[0] = y1;
		sc->sc_page_range[1] = y2;
		reg[0] = SSDFB_SET_PAGE_RANGE;
		reg[1] = sc->sc_page_range[0];
		reg[2] = sc->sc_page_range[1];
		ssdfb_write_command(sc, reg, 3);
	}
}

void
ssdfb_update(void *v)
{
	struct ssdfb_softc *sc = v;

	ssdfb_partial(sc, 0, SSDFB_WIDTH, 0, SSDFB_HEIGHT);
	timeout_add_msec(&sc->sc_to, 100);
}

void
ssdfb_partial(struct ssdfb_softc *sc, uint32_t x1, uint32_t x2,
    uint32_t y1, uint32_t y2)
{
	struct rasops_info *ri = &sc->sc_rinfo;
	uint32_t off, width, height;
	uint8_t *bit, val;
	int i, j, k;

	if (x2 < x1 || y2 < y1)
		return;

	if (x2 > SSDFB_WIDTH || y2 > SSDFB_HEIGHT)
		return;

	y1 = y1 & ~0x7;
	y2 = roundup(y2, 8);

	width = x2 - x1;
	height = y2 - y1;

	memset(sc->sc_fb, 0, (width * height) / 8);

	for (i = 0; i < height; i += 8) {
		for (j = 0; j < width; j++) {
			bit = &sc->sc_fb[(i / 8) * width + j];
			for (k = 0; k < 8; k++) {
				off = ri->ri_stride * (y1 + i + k);
				off += (x1 + j) / 8;
				val = *(ri->ri_bits + off);
				val &= (1 << ((x1 + j) % 8));
				*bit |= !!val << k;
			}
		}
	}

	ssdfb_set_range(sc, x1, x2 - 1, y1 / 8, (y2 / 8) - 1);
	ssdfb_write_data(sc, sc->sc_fb, (width * height) / 8);
}

void
ssdfb_timeout(void *v)
{
	struct ssdfb_softc *sc = v;
	task_add(systq, &sc->sc_task);
}

void
ssdfb_write_command(struct ssdfb_softc *sc, char *buf, size_t len)
{
	return sc->sc_write_command(sc, buf, len);
}

void
ssdfb_write_data(struct ssdfb_softc *sc, char *buf, size_t len)
{
	return sc->sc_write_data(sc, buf, len);
}

void
ssdfb_i2c_write_command(struct ssdfb_softc *sc, char *buf, size_t len)
{
	uint8_t type;

	type = SSDFB_I2C_COMMAND;
	iic_acquire_bus(sc->sc_i2c_tag, 0);
	if (iic_exec(sc->sc_i2c_tag, I2C_OP_WRITE_WITH_STOP,
	    sc->sc_i2c_addr, &type, sizeof(type), buf, len, 0)) {
		printf("%s: cannot write\n", sc->sc_dev.dv_xname);
	}
	iic_release_bus(sc->sc_i2c_tag, 0);
}

void
ssdfb_i2c_write_data(struct ssdfb_softc *sc, char *buf, size_t len)
{
	uint8_t type;

	type = SSDFB_I2C_DATA;
	iic_acquire_bus(sc->sc_i2c_tag, 0);
	if (iic_exec(sc->sc_i2c_tag, I2C_OP_WRITE_WITH_STOP,
	    sc->sc_i2c_addr, &type, sizeof(type), buf, len, 0)) {
		printf("%s: cannot write\n", sc->sc_dev.dv_xname);
	}
	iic_release_bus(sc->sc_i2c_tag, 0);
}

void
ssdfb_spi_write_command(struct ssdfb_softc *sc, char *buf, size_t len)
{
	if (sc->sc_cd != 0) {
		gpio_controller_set_pin(sc->sc_gpio, 0);
		sc->sc_cd = 0;
		delay(1);
	}

	spi_acquire_bus(sc->sc_spi_tag, 0);
	spi_config(sc->sc_spi_tag, &sc->sc_spi_conf);
	if (spi_write(sc->sc_spi_tag, buf, len))
		printf("%s: cannot write\n", sc->sc_dev.dv_xname);
	spi_release_bus(sc->sc_spi_tag, 0);
}

void
ssdfb_spi_write_data(struct ssdfb_softc *sc, char *buf, size_t len)
{
	if (sc->sc_cd != 1) {
		gpio_controller_set_pin(sc->sc_gpio, 1);
		sc->sc_cd = 1;
		delay(1);
	}

	spi_acquire_bus(sc->sc_spi_tag, 0);
	spi_config(sc->sc_spi_tag, &sc->sc_spi_conf);
	if (spi_write(sc->sc_spi_tag, buf, len))
		printf("%s: cannot write\n", sc->sc_dev.dv_xname);
	spi_release_bus(sc->sc_spi_tag, 0);
}

int
ssdfb_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct ssdfb_softc	*sc = v;
	struct rasops_info 	*ri = &sc->sc_rinfo;
	struct wsdisplay_fbinfo	*wdf;

	switch (cmd) {
	case WSDISPLAYIO_GETPARAM:
	case WSDISPLAYIO_SETPARAM:
		return (-1);
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_UNKNOWN;
		break;
	case WSDISPLAYIO_GINFO:
		wdf = (struct wsdisplay_fbinfo *)data;
		wdf->width = ri->ri_width;
		wdf->height = ri->ri_height;
		wdf->depth = ri->ri_depth;
		wdf->cmsize = 0;	/* color map is unavailable */
		break;
	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = ri->ri_stride;
		break;
	case WSDISPLAYIO_SMODE:
		break;
	case WSDISPLAYIO_GETSUPPORTEDDEPTH:
		*(u_int *)data = WSDISPLAYIO_DEPTH_1;
		break;
	default:
		return (-1);
	}

	return (0);
}

paddr_t
ssdfb_mmap(void *v, off_t off, int prot)
{
	return -1;
}

int
ssdfb_alloc_screen(void *v, const struct wsscreen_descr *descr,
    void **cookiep, int *curxp, int *curyp, long *attrp)
{
	struct ssdfb_softc	*sc = v;
	struct rasops_info	*ri = &sc->sc_rinfo;

	return rasops_alloc_screen(ri, cookiep, curxp, curyp, attrp);
}

void
ssdfb_free_screen(void *v, void *cookie)
{
	struct ssdfb_softc	*sc = v;
	struct rasops_info	*ri = &sc->sc_rinfo;

	rasops_free_screen(ri, cookie);
}

int
ssdfb_show_screen(void *v, void *cookie, int waitok,
    void (*cb) (void *, int, int), void *cb_arg)
{
	struct ssdfb_softc	*sc = v;
	struct rasops_info	*ri = &sc->sc_rinfo;

	return rasops_show_screen(ri, cookie, waitok, cb, cb_arg);
}

int
ssdfb_load_font(void *v, void *cookie, struct wsdisplay_font *font)
{
	struct ssdfb_softc	*sc = v;
	struct rasops_info	*ri = &sc->sc_rinfo;

	return (rasops_load_font(ri, cookie, font));
}

int
ssdfb_list_font(void *v, struct wsdisplay_font *font)
{
	struct ssdfb_softc	*sc = v;
	struct rasops_info	*ri = &sc->sc_rinfo;

	return (rasops_list_font(ri, font));
}
