/*
 * Copyright 2018 NXP
 * Copyright 2019 Sony Home Entertainment & Sound Products Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <imx_mipi_dsi_bridge.h>
#include <mipi_display.h>
#include <linux/compat.h>

#define WRDISBV		0x51
#define WRCTRLD		0x53
#define WRCABC		0x55
#define SETPOWER	0xb1
#define SETDISP		0xb2
#define SETCYC		0xb4
#define SETVCOM		0xb6
#define SETEXTC		0xb9
#define SETMIPI		0xba
#define SETPANEL	0xcc
#define SETGAMMA	0xe0

#define SETTCON		0xc7
#define SETOFFSET	0xd2
#define SETCE		0xe4
#define SETGIP_0	0xd3
#define SETGIP_1	0xd5
#define SETGIP_2	0xd6


struct hx8379c {
	u8 tmp;
};

struct hx8379c static_hx8379c;

static int mipi_dsi_generic_write(const void *payload, size_t size)
{
	int ret;
	u16 tx_buf;
	u8 *tx;

	tx_buf = 0;
	tx = (u8 *)&tx_buf;

	switch (size) {
	case 0:
		ret = imx_mipi_dsi_bridge_pkt_write(MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM, tx, 0);
		break;

	case 1:
		tx[0] = *(u8 *)payload;
		ret = imx_mipi_dsi_bridge_pkt_write(MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM, tx, 0);
		break;

	case 2:
		if(0xb0 <= ((u8*)payload)[0]){ 
			printf("force change type %x\n",((u8*)payload)[0]); 
			ret = imx_mipi_dsi_bridge_pkt_write(MIPI_DSI_GENERIC_LONG_WRITE, (const u8 *)payload, size);
		} 
		ret = imx_mipi_dsi_bridge_pkt_write(MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM, (const u8 *)payload, 0);
		break;

	default:
		ret = imx_mipi_dsi_bridge_pkt_write(MIPI_DSI_GENERIC_LONG_WRITE, (const u8 *)payload, size);
		break;
	}

	return ret;
}

static int mipi_dsi_dcs_write(u8 cmd, const void *data, size_t len)
{
	u32 size;
	u8 *tx;
	u16 tx_buf;
	int ret;

	if (len > 0) {
		size = 1 + len;

		tx = kmalloc(size, GFP_KERNEL);
		if (!tx)
			return -ENOMEM;

		/* concatenate the DCS command byte and the payload */
		tx[0] = cmd;
		memcpy(&tx[1], data, len);
	} else {
		tx = (u8 *)&tx_buf;
		tx[0] = cmd;
		tx[1] = 0;
		size = 1;
	}

	switch (size) {
	case 1:
		ret = imx_mipi_dsi_bridge_pkt_write(MIPI_DSI_DCS_SHORT_WRITE, tx, 0);
		break;

	case 2:
		ret = imx_mipi_dsi_bridge_pkt_write(MIPI_DSI_DCS_SHORT_WRITE_PARAM, tx, 0);
		break;

	default:
		ret = imx_mipi_dsi_bridge_pkt_write(MIPI_DSI_DCS_LONG_WRITE, tx, size);
		break;
	}

	if (len > 0)
		kfree(tx);

	return ret;
}

static void hx8379c_dcs_write(const char *func,
			      const void *data, size_t len)
{
	ssize_t ret;
	ret = mipi_dsi_generic_write(data, len);
	if (ret < 0)
		printf("%s failed: %ld\n", func, ret);

	printf("##### %s: %d \n",__func__,__LINE__);
}



#define hx8379c_dcs_write_seq_static(ctx, seq...) \
({ \
	static const u8 d[] = { seq }; \
	hx8379c_dcs_write(__func__, d, ARRAY_SIZE(d)); \
})

static void hx8379c_dsi_set_display_related_register(struct hx8379c *ctx)
{
	//u8 sec_p = (ctx->res_sel << 4) | 0x03;
	printf("##### %s: %d\n",__func__,__LINE__);
	hx8379c_dcs_write_seq_static(ctx, SETDISP, 0x80, 0x3c, 0x0a,
		0x03, 0x70, 0x50, 0x11, 0x42, 0x1d);
}

static void hx8379c_dsi_set_display_waveform_cycle(struct hx8379c *ctx)
{
	printf("##### %s: %d\n",__func__,__LINE__);
	hx8379c_dcs_write_seq_static(ctx, SETCYC, 0x02, 0x7c, 0x02, 0x7c, 0x02,
		0x7c, 0x22, 0x86, 0x23, 0x86);
}

static void hx8379c_dsi_set_tcon(struct hx8379c *ctx)
{
	printf("##### %s: %d\n",__func__,__LINE__);
	hx8379c_dcs_write_seq_static(ctx, SETTCON, 0x00, 0x00, 0x00, 0xC0);
}
static void hx8379c_dsi_set_offset(struct hx8379c *ctx)
{
	printf("##### %s: %d\n",__func__,__LINE__);
	hx8379c_dcs_write_seq_static(ctx, SETOFFSET, 0x77);
}
static void hx8379c_dsi_set_color_enhancement(struct hx8379c *ctx)
{
	printf("##### %s: %d\n",__func__,__LINE__);
	hx8379c_dcs_write_seq_static(ctx, SETCE, 0x00,0x01);
}


static void hx8379c_dsi_set_gip_0(struct hx8379c *ctx)
{
	printf("##### %s: %d\n",__func__,__LINE__);
	hx8379c_dcs_write_seq_static(ctx, SETGIP_0, 0x00, 0x07, 0x00, 0x00, 0x00,
				0x08, 0x08, 0x32, 0x10, 0x01, 0x00, 0x01, 0x03,
				0x72, 0x03, 0x72, 0x00, 0x08, 0x00, 0x08, 0x33,
				0x33, 0x05, 0x05, 0x37, 0x05, 0x05, 0x37, 0x08,
				0x00, 0x00, 0x00, 0x0a, 0x00, 0x01, 0x01, 0x0f);
	
}
static void hx8379c_dsi_set_gip_1(struct hx8379c *ctx)
{
	printf("##### %s: %d\n",__func__,__LINE__);
	hx8379c_dcs_write_seq_static(ctx, SETGIP_1, 0x18, 0x18, 0x18, 0x18,
                0x18, 0x18, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02,
                0x01, 0x00, 0x18, 0x18, 0x21, 0x20, 0x18, 0x18,
                0x19, 0x19, 0x23, 0x22, 0x38, 0x38, 0x78, 0x78,
                0x18, 0x18, 0x18, 0x18, 0x00, 0x00);
}
static void hx8379c_dsi_set_gip_2(struct hx8379c *ctx)
{
	printf("##### %s: %d\n",__func__,__LINE__);
	hx8379c_dcs_write_seq_static(ctx, SETGIP_2, 0x18, 0x18, 0x18, 0x18,
                0x18, 0x18, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                0x06, 0x07, 0x18, 0x18, 0x22, 0x23, 0x19, 0x19,
                0x18, 0x18, 0x20, 0x21, 0x38, 0x38, 0x38, 0x38,
                0x18, 0x18, 0x18, 0x18);
}

static void hx8379c_dsi_set_power(struct hx8379c *ctx)
{
	printf("##### %s: %d\n",__func__,__LINE__);
	hx8379c_dcs_write_seq_static(ctx, SETPOWER, 0x44, 0x18, 0x18, 0x31, 0x51,
				0x50, 0xd0, 0xd8, 0x58, 0x80, 0x38, 0x38, 0xf8,
				0x33, 0x32, 0x22);
}

static void hx8379c_dsi_set_vcom_voltage(struct hx8379c *ctx)
{
	printf("##### %s: %d\n",__func__,__LINE__);
	hx8379c_dcs_write_seq_static(ctx, SETVCOM, 0x5e, 0x5e);
}

static void hx8379c_dsi_set_panel(struct hx8379c *ctx)
{
	printf("##### %s: %d\n",__func__,__LINE__);
	hx8379c_dcs_write_seq_static(ctx, SETPANEL, 0x02);
}

static void hx8379c_dsi_set_gamma_curve(struct hx8379c *ctx)
{
	printf("##### %s: %d\n",__func__,__LINE__);
	hx8379c_dcs_write_seq_static(ctx, SETGAMMA, 0x00, 0x01, 0x04, 0x20,
                0x24, 0x3f, 0x11, 0x33, 0x09, 0x0a, 0x0c, 0x17,
                0x0f, 0x12, 0x15, 0x13, 0x14, 0x0a, 0x15, 0x16,
                0x18, 0x00, 0x01, 0x04, 0x20, 0x24, 0x3f, 0x11,
                0x33, 0x09, 0x0B, 0x0c, 0x17, 0x0e, 0x11, 0x14,
                0x13, 0x14, 0x0a, 0x15, 0x16, 0x18);
}

static void hx8379c_dsi_write_cabc(struct hx8379c *ctx)
{
	printf("##### %s: %d\n",__func__,__LINE__);
	hx8379c_dcs_write_seq_static(ctx, WRCABC, 0x01);
}

static void hx8379c_dsi_write_control_display(struct hx8379c *ctx)
{
	printf("##### %s: %d\n",__func__,__LINE__);
	hx8379c_dcs_write_seq_static(ctx, WRCTRLD, 0x24);
}


static void hx8379c_dsi_panel_init(struct hx8379c *ctx)
{
	printf("##### %s: %d\n",__func__,__LINE__);
	hx8379c_dsi_set_power(ctx);
	hx8379c_dsi_set_display_related_register(ctx);
	hx8379c_dsi_set_display_waveform_cycle(ctx);
	hx8379c_dsi_set_tcon(ctx);
	hx8379c_dsi_set_panel(ctx);
	hx8379c_dsi_set_offset(ctx);
	hx8379c_dsi_set_gip_0(ctx);
	hx8379c_dsi_set_gip_1(ctx);
	hx8379c_dsi_set_gip_2(ctx);
	hx8379c_dsi_set_gamma_curve(ctx);
	hx8379c_dsi_set_vcom_voltage(ctx);
}

static void hx8379c_dsi_set_extension_command(struct hx8379c *ctx)
{
	printf("##### %s: %d\n",__func__,__LINE__);
	hx8379c_dcs_write_seq_static(ctx, SETEXTC, 0xff, 0x83, 0x79);
	mdelay(10);
}

int hx8379c_lcd_setup(struct mipi_dsi_client_dev *panel_dev)
{

	int ret;
	struct hx8379c *ctx = &static_hx8379c;

	hx8379c_dsi_set_extension_command(ctx);
	hx8379c_dsi_panel_init(ctx);

	/* Set tear ON */
	ret = mipi_dsi_dcs_write(MIPI_DCS_SET_TEAR_ON, (u8[]){ 0x0 }, 1);
	if (ret < 0) {
		printf("Failed to set tear ON (%d)\n", ret);
		return -EIO;
	}

	/* Exit sleep mode */
	ret = mipi_dsi_dcs_write(MIPI_DCS_EXIT_SLEEP_MODE, NULL, 0);
	if (ret < 0) {
		printf("Failed to exit sleep mode (%d)\n", ret);
		return -EIO;
	}

	mdelay(120);

	ret = mipi_dsi_dcs_write(MIPI_DCS_SET_DISPLAY_ON, NULL, 0);
	if (ret < 0) {
		printf("Failed to set display ON (%d)\n", ret);
		return -EIO;
	}

	mdelay(120);

	return 0;
}

static struct mipi_dsi_client_driver hx8379c_drv = {
	.name = "HX8379C_LCD",
	.dsi_client_setup = hx8379c_lcd_setup,
};

void hx8379c_init(void)
{
	imx_mipi_dsi_bridge_add_client_driver(&hx8379c_drv);
}