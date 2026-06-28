/*
 * Copyright 2018 NXP
 * Copyright 2018,2019 Sony Video & Sound Products Inc.
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
#define SETBANK		0xbd
#define SETPTBA		0xbf
#define SETPANEL	0xcc
#define SETGAMMA	0xe0

#define SETTCON		0xc7
#define SETOFFSET	0xd2
#define SETCE		0xe4
#define SETGIP_0	0xd3
#define SETGIP_1	0xd5
#define SETGIP_2	0xd6

#define SPECIFIC_B0	0xb0
#define SPECIFIC_CB	0xcb
#define SPECIFIC_E7	0xe7

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
		ret = imx_mipi_dsi_bridge_pkt_write(MIPI_DSI_DCS_SHORT_WRITE_PARAM, tx, 0);
		break;

	case 2:
		if(0xb0 <= ((u8*)payload)[0]){
			ret = imx_mipi_dsi_bridge_pkt_write(MIPI_DSI_DCS_LONG_WRITE, (const u8 *)payload, size);
			break;
		}
		ret = imx_mipi_dsi_bridge_pkt_write(MIPI_DSI_DCS_SHORT_WRITE_PARAM, (const u8 *)payload, 0);
		break;

	default:
		ret = imx_mipi_dsi_bridge_pkt_write(MIPI_DSI_DCS_LONG_WRITE, (const u8 *)payload, size);
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

static int hx83102d_dcs_write(const char *func,
			      const void *data, size_t len)
{
	int ret;

	ret = mipi_dsi_generic_write(data, len);
	if (ret < 0)
		printf("%s failed: %d\n", func, ret);
	return ret;
}


#define hx83102d_dcs_write_seq_static(seq...) \
({ \
	static const u8 d[] = { seq }; \
	hx83102d_dcs_write(__func__, d, ARRAY_SIZE(d)); \
})

static void hx83102d_dsi_panel_init(void)
{
	//set_power
	hx83102d_dcs_write_seq_static(SETPOWER, 0x20, 0x11, 0x27, 0x27,
			0x22, 0x77, 0x2f, 0x43, 0x18, 0x18, 0x18);
	//set_display_related_register
	hx83102d_dcs_write_seq_static(SETDISP, 0x00, 0x00, 0x05,
		0x00, 0x00, 0x0E, 0x92, 0x4D, 0x00, 0x00, 0x00,
		0x00, 0x14,0x20);
	//set_display_waveform_cycle
	hx83102d_dcs_write_seq_static(SETCYC, 0x14, 0x60, 0x14, 0x60, 0x14,
		0x60, 0x14, 0x60, 0x03, 0xff, 0x01, 0x20, 0x00, 0xff);

	//set_panel
	hx83102d_dcs_write_seq_static(SETPANEL, 0x02);
	//set_gip_0
	hx83102d_dcs_write_seq_static(SETGIP_0, 0x33, 0x00, 0x3c, 0x03, 0x00,
		0x08, 0x00, 0x37, 0x00, 0x33, 0x3b, 0x0a, 0x0a,
		0x00, 0x00, 0x32, 0x10, 0x06, 0x00, 0x06, 0x00,
		0x00, 0x02, 0x00, 0x02, 0x00, 0x05, 0x15, 0x05,
		0x15, 0x00, 0x00, 0x00, 0x1a, 0x00, 0x00, 0x00,
		0x1a, 0x00, 0x00);
	//set_gip_1
	hx83102d_dcs_write_seq_static(SETGIP_1, 0x18, 0x18, 0x18, 0x18,
		0x19, 0x19, 0x39, 0x39, 0x24, 0x24, 0x04, 0x05,
		0x06, 0x07, 0x00, 0x01, 0x02, 0x03, 0x18, 0x18,
		0x38, 0x38, 0x20, 0x21, 0x22, 0x23, 0x18, 0x18,
		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18);
	//set_gip_2
	hx83102d_dcs_write_seq_static(SETGIP_2, 0x18, 0x18, 0x19, 0x19,
		0x18, 0x18, 0x39, 0x39, 0x24, 0x24, 0x03, 0x02,
		0x01, 0x00, 0x07, 0x06, 0x05, 0x04, 0x18, 0x18,
		0x38, 0x38, 0x23, 0x22, 0x21, 0x20, 0x18, 0x18,
		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18);
	//set_specific_e7
	hx83102d_dcs_write_seq_static(SPECIFIC_E7, 0xff, 0x0f, 0x00, 0x00);
	//set_bank_1
	hx83102d_dcs_write_seq_static(SETBANK, 0x01);
	//set_specific_e7
	hx83102d_dcs_write_seq_static(SPECIFIC_E7, 0x01);
	//set_bank_0
	hx83102d_dcs_write_seq_static(SETBANK, 0x00);
	//set_vcom_voltage
	hx83102d_dcs_write_seq_static(SETVCOM, 0x90, 0x90, 0xe0);
	//set_gamma_curve
	hx83102d_dcs_write_seq_static(SETGAMMA, 0x00, 0x05, 0x0F, 0x17,
		0x1F, 0x35, 0x4F, 0x56, 0x5C, 0x5B, 0x75, 0x7B,
		0x82, 0x92, 0x92, 0x9C, 0xA6, 0xBC, 0xBC, 0x5E,
		0x65, 0x6F, 0x7F, 0x00, 0x05, 0x0F, 0x17, 0x1F,
		0x35, 0x4F, 0x56, 0x5C, 0x5B, 0x75, 0x7B, 0x82,
		0x92, 0x92, 0x9C, 0xA6, 0xBC, 0xBC, 0x5E, 0x65,
		0x6F, 0x7F);
	//set_specific_b0
	hx83102d_dcs_write_seq_static(SPECIFIC_B0, 0x00, 0x20);
	//set_mipi
	hx83102d_dcs_write_seq_static(SETMIPI, 0x70, 0x23,
	 0xA8, 0x93, 0xB2, 0x80, 0x80, 0x01, 0x10, 0x00, 0x00,
	  0x00, 0x0C, 0x3D, 0x82, 0x77, 0x04, 0x01, 0x04);
	//set_bank_1
	hx83102d_dcs_write_seq_static(SETBANK, 0x01);
	//set_specific_cb
	hx83102d_dcs_write_seq_static(SPECIFIC_CB, 0x01);
	//set_bank_0
	hx83102d_dcs_write_seq_static(SETBANK, 0x00);
	//set_power_option
	hx83102d_dcs_write_seq_static(SETPTBA, 0xFC, 0x00,
	 0x05, 0x9E, 0xF6, 0x00, 0x45);
	//set_bank_2
	hx83102d_dcs_write_seq_static(SETBANK, 0x02);
	//set_display_waveform_cycle_2
	hx83102d_dcs_write_seq_static(SETCYC, 0xC2, 0x00, 0x33, 0x00, 0x33,
							 0x88, 0xB3, 0x00);
	//set_bank_0
	hx83102d_dcs_write_seq_static(SETBANK, 0x00);
	//set_bank_1
	hx83102d_dcs_write_seq_static(SETBANK, 0x01);
	//set_gip_0
	hx83102d_dcs_write_seq_static(SETGIP_0, 0x09, 0x00, 0x78);
	//set_bank_0
	hx83102d_dcs_write_seq_static(SETBANK, 0x00);
	//set_bank_2
	hx83102d_dcs_write_seq_static(SETBANK, 0x02);
	//set_power
	hx83102d_dcs_write_seq_static(SETPOWER, 0x7F, 0x07, 0xFF);
	//set_bank_0
	hx83102d_dcs_write_seq_static(SETBANK, 0x00);
}

static int hx83102d_dsi_set_extension_command(void)
{
	int ret;
	const u8 hx83102d_extension_command[] = { SETEXTC, 0x83, 0x10, 0x2d };

	ret = hx83102d_dcs_write(__func__, hx83102d_extension_command,
				ARRAY_SIZE(hx83102d_extension_command));
	if (ret < 0) {
		printf("%s failed: %d\n", __func__, ret);
		return ret;
	}
	udelay(10000);
	return ret;
}

int hx83102d_lcd_setup(struct mipi_dsi_client_dev *panel_dev)
{

	int ret;

	ret = hx83102d_dsi_set_extension_command();
	if (ret < 0) {
		printf("Failed to set hx83102d_dsi_set_extension_command(%d)\n",
									 ret);
		return -EIO;
	}

	hx83102d_dsi_panel_init();

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

	mdelay(130);

	ret = mipi_dsi_dcs_write(MIPI_DCS_SET_DISPLAY_ON, NULL, 0);
	if (ret < 0) {
		printf("Failed to set display ON (%d)\n", ret);
		return -EIO;
	}

	mdelay(120);

	return 0;
}

static struct mipi_dsi_client_driver hx83102d_drv = {
	.name = "HX83102D_LCD",
	.dsi_client_setup = hx83102d_lcd_setup,
};

void hx83102d_init(void)
{
	imx_mipi_dsi_bridge_add_client_driver(&hx83102d_drv);
}
