/*	$OpenBSD: ofw_misc.h,v 1.19 2021/02/28 21:09:44 patrick Exp $	*/
/*
 * Copyright (c) 2017 Mark Kettenis
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

#ifndef _DEV_OFW_MISC_H_
#define _DEV_OFW_MISC_H_

/* Register maps */

void	regmap_register(int, bus_space_tag_t, bus_space_handle_t, bus_size_t);

struct regmap;
struct regmap *regmap_bycompatible(char *);
struct regmap *regmap_bynode(int);
struct regmap *regmap_byphandle(uint32_t);

uint32_t regmap_read_4(struct regmap *, bus_size_t);
void	regmap_write_4(struct regmap *, bus_size_t, uint32_t);

/* PHY support */

#define PHY_NONE	0
#define PHY_TYPE_SATA	1
#define PHY_TYPE_PCIE	2
#define PHY_TYPE_USB2	3
#define PHY_TYPE_USB3	4
#define PHY_TYPE_UFS	5

struct phy_device {
	int	pd_node;
	void	*pd_cookie;
	int	(*pd_enable)(void *, uint32_t *);

	LIST_ENTRY(phy_device) pd_list;
	uint32_t pd_phandle;
	uint32_t pd_cells;
};

void	phy_register(struct phy_device *);

int	phy_enable_idx(int, int);
int	phy_enable(int, const char *);

/* I2C support */

struct i2c_controller;
struct i2c_bus {
	int			ib_node;
	struct i2c_controller	*ib_ic;

	LIST_ENTRY(i2c_bus) ib_list;
	uint32_t ib_phandle;
};

void	i2c_register(struct i2c_bus *);

struct i2c_controller *i2c_bynode(int);
struct i2c_controller *i2c_byphandle(uint32_t);

/* SFP support */

struct if_sffpage;
struct sfp_device {
	int	sd_node;
	void	*sd_cookie;
	int	(*sd_enable)(void *, int);
	int	(*sd_get_sffpage)(void *, struct if_sffpage *);

	LIST_ENTRY(sfp_device) sd_list;
	uint32_t sd_phandle;
};

void	sfp_register(struct sfp_device *);

struct mii_data;
int	sfp_enable(uint32_t);
int	sfp_disable(uint32_t);
int	sfp_add_media(uint32_t, struct mii_data *);
int	sfp_get_sffpage(uint32_t, struct if_sffpage *);

/* PWM support */

#define PWM_POLARITY_INVERTED	0x00000001

struct pwm_state {
	uint32_t ps_period;
	uint32_t ps_pulse_width;
	uint32_t ps_flags;
	int ps_enabled;
};

struct pwm_device {
	int	pd_node;
	void	*pd_cookie;
	int	(*pd_get_state)(void *, uint32_t *, struct pwm_state *);
	int	(*pd_set_state)(void *, uint32_t *, struct pwm_state *);

	LIST_ENTRY(pwm_device) pd_list;
	uint32_t pd_phandle;
	uint32_t pd_cells;
};

void	pwm_register(struct pwm_device *);

int	pwm_init_state(uint32_t *cells, struct pwm_state *ps);
int	pwm_get_state(uint32_t *cells, struct pwm_state *ps);
int	pwm_set_state(uint32_t *cells, struct pwm_state *ps);

/* Non-volatile memory support */

struct nvmem_device {
	int	nd_node;
	void	*nd_cookie;
	int	(*nd_read)(void *, bus_addr_t, void *, bus_size_t);

	LIST_ENTRY(nvmem_device) nd_list;
	uint32_t nd_phandle;
};

void	nvmem_register(struct nvmem_device *);
int	nvmem_read(uint32_t, bus_addr_t, void *, bus_size_t);
int	nvmem_read_cell(int, const char *name, void *, bus_size_t);

/* Port/endpoint interface support */

struct endpoint;

struct device_ports {
	int	dp_node;
	void	*dp_cookie;

	int	(*dp_ep_activate)(void *, struct endpoint *, void *);
	void	*(*dp_ep_get_cookie)(void *, struct endpoint *);

	LIST_HEAD(, device_port) dp_ports;
};

struct device_port {
	int	dp_node;
	uint32_t dp_phandle;
	uint32_t dp_reg;
	struct device_ports *dp_ports;
	LIST_ENTRY(device_port) dp_list;
	LIST_HEAD(, endpoint) dp_endpoints;
};

enum endpoint_type {
	EP_DRM_BRIDGE = 1,	/* struct drm_bridge */
	EP_DRM_CONNECTOR,	/* struct drm_connector */
	EP_DRM_CRTC,		/* struct drm_crtc */
	EP_DRM_ENCODER,		/* struct drm_encoder */
	EP_DRM_PANEL,		/* struct drm_panel */
	EP_DAI_DEVICE,		/* struct dai_device */
};

struct endpoint {
	int ep_node;
	uint32_t ep_phandle;
	uint32_t ep_reg;
	enum endpoint_type ep_type;
	struct device_port *ep_port;
	LIST_ENTRY(endpoint) ep_list;
	LIST_ENTRY(endpoint) ep_plist;
};

void	device_ports_register(struct device_ports *, enum endpoint_type);
struct device_ports *device_ports_byphandle(uint32_t);
int	device_port_activate(uint32_t, void *);
struct endpoint *endpoint_byreg(struct device_ports *, uint32_t, uint32_t);
struct endpoint *endpoint_remote(struct endpoint *);
int	endpoint_activate(struct endpoint *, void *);
void	*endpoint_get_cookie(struct endpoint *);

/* Digital audio interface support */

struct dai_device {
	int	dd_node;
	void	*dd_cookie;
	void	*dd_hw_if;
	int	(*dd_set_format)(void *, uint32_t, uint32_t, uint32_t);
	int	(*dd_set_sysclk)(void *, uint32_t);

	LIST_ENTRY(dai_device) dd_list;
	uint32_t dd_phandle;

	struct device_ports dd_ports;
};

void	dai_register(struct dai_device *);
struct dai_device *dai_byphandle(uint32_t);

#define DAI_FORMAT_I2S			0
#define DAI_FORMAT_RJ			1
#define DAI_FORMAT_LJ			2
#define DAI_FORMAT_DSPA			3
#define DAI_FORMAT_DSPB			4
#define DAI_FORMAT_AC97			5
#define DAI_FORMAT_PDM			6
#define DAI_FORMAT_MSB			7
#define DAI_FORMAT_LSB			8

#define DAI_POLARITY_NB			(0 << 0)
#define DAI_POLARITY_IB			(1 << 0)
#define DAI_POLARITY_NF			(0 << 1)
#define DAI_POLARITY_IF			(1 << 1)

#define DAI_CLOCK_CBS			(0 << 0)
#define DAI_CLOCK_CBM			(1 << 0)
#define DAI_CLOCK_CFS			(0 << 1)
#define DAI_CLOCK_CFM			(1 << 1)

/* MII support */

struct mii_bus {
	int	md_node;
	void	*md_cookie;
	int	(*md_readreg)(struct device *, int, int);
	void	(*md_writereg)(struct device *, int, int, int);

	LIST_ENTRY(mii_bus) md_list;
};

void	mii_register(struct mii_bus *);
struct mii_bus *mii_bynode(int);
struct mii_bus *mii_byphandle(uint32_t);

/* IOMMU support */

struct iommu_device {
	int	id_node;
	void	*id_cookie;
	bus_dma_tag_t (*id_map)(void *, uint32_t *, bus_dma_tag_t);

	LIST_ENTRY(iommu_device) id_list;
	uint32_t id_phandle;
};

void	iommu_device_register(struct iommu_device *);
bus_dma_tag_t iommu_device_map(int, bus_dma_tag_t);
bus_dma_tag_t iommu_device_map_pci(int, uint32_t, bus_dma_tag_t);

#endif /* _DEV_OFW_MISC_H_ */
