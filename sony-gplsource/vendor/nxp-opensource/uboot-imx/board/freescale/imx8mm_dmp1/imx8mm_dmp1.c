/*
 * Copyright 2018 NXP
 * Copyright 2018,2019 Sony Video & Sound Products Inc.
 * Copyright 2019,2020 Sony Home Entertainment & Sound Products Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <div64.h>
#include <malloc.h>
#include <errno.h>
#include <asm/io.h>
#include <miiphy.h>
#include <netdev.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm-generic/gpio.h>
#include <fsl_esdhc.h>
#include <mmc.h>
#include <asm/arch/imx8mm_pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/mach-imx/gpio.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <asm/arch/clock.h>
#include <spl.h>
#include <asm/mach-imx/dma.h>
#include <power/pmic.h>
#include <power/bd71837.h>
#include "../common/tcpc.h"
#include <usb.h>
#include <sec_mipi_dsim.h>
#include <imx_mipi_dsi_bridge.h>
#include <mipi_dsi_panel.h>
#include <asm/mach-imx/video.h>
#include <nvp.h>
#include <icx_port_init.h>
#include <icx_dmp_board_id.h>
#include <icx_fastboot.h>
#include <power/sony_max1704x.h>
#include <asm/arch/ddr.h>

DECLARE_GLOBAL_DATA_PTR;

#define UART_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_FSEL1)
#define WDOG_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_ODE | PAD_CTL_PUE | PAD_CTL_PE)

static iomux_v3_cfg_t const uart_pads[] = {
	IMX8MM_PAD_UART2_RXD_UART2_RX | MUX_PAD_CTRL(UART_PAD_CTRL),
	IMX8MM_PAD_UART2_TXD_UART2_TX | MUX_PAD_CTRL(UART_PAD_CTRL),
};

#define CONSOLE_RXD_GPIO   IMX_GPIO_NR(5, 24)
#define CONSOLE_TXD_GPIO   IMX_GPIO_NR(5, 25)
#define CONSOLE_GPIO_PAD_CTRL (PAD_CTL_ODE)

static iomux_v3_cfg_t const console_gpio_pads[] = {
	IMX8MM_PAD_UART2_RXD_GPIO5_IO24 | MUX_PAD_CTRL(CONSOLE_GPIO_PAD_CTRL),
	IMX8MM_PAD_UART2_TXD_GPIO5_IO25 | MUX_PAD_CTRL(CONSOLE_GPIO_PAD_CTRL),
};

static iomux_v3_cfg_t const wdog_pads[] = {
	IMX8MM_PAD_GPIO1_IO02_WDOG1_WDOG_B  | MUX_PAD_CTRL(WDOG_PAD_CTRL),
};

#ifdef CONFIG_FSL_FSPI
#define QSPI_PAD_CTRL	(PAD_CTL_DSE2 | PAD_CTL_HYS)
static iomux_v3_cfg_t const qspi_pads[] = {
	IMX8MM_PAD_NAND_ALE_QSPI_A_SCLK | MUX_PAD_CTRL(QSPI_PAD_CTRL | PAD_CTL_PE | PAD_CTL_PUE),
	IMX8MM_PAD_NAND_CE0_B_QSPI_A_SS0_B | MUX_PAD_CTRL(QSPI_PAD_CTRL),

	IMX8MM_PAD_NAND_DATA00_QSPI_A_DATA0 | MUX_PAD_CTRL(QSPI_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA01_QSPI_A_DATA1 | MUX_PAD_CTRL(QSPI_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA02_QSPI_A_DATA2 | MUX_PAD_CTRL(QSPI_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA03_QSPI_A_DATA3 | MUX_PAD_CTRL(QSPI_PAD_CTRL),
};

int board_qspi_init(void)
{
	imx_iomux_v3_setup_multiple_pads(qspi_pads, ARRAY_SIZE(qspi_pads));

	set_clk_qspi();

	return 0;
}
#endif

#ifdef CONFIG_MXC_SPI
#define SPI_PAD_CTRL	(PAD_CTL_DSE2 | PAD_CTL_HYS)
static iomux_v3_cfg_t const ecspi1_pads[] = {
	IMX8MM_PAD_ECSPI1_SCLK_ECSPI1_SCLK | MUX_PAD_CTRL(SPI_PAD_CTRL),
	IMX8MM_PAD_ECSPI1_MOSI_ECSPI1_MOSI | MUX_PAD_CTRL(SPI_PAD_CTRL),
	IMX8MM_PAD_ECSPI1_MISO_ECSPI1_MISO | MUX_PAD_CTRL(SPI_PAD_CTRL),
	IMX8MM_PAD_ECSPI1_SS0_GPIO5_IO9 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

static iomux_v3_cfg_t const ecspi2_pads[] = {
	IMX8MM_PAD_ECSPI2_SCLK_ECSPI2_SCLK | MUX_PAD_CTRL(SPI_PAD_CTRL),
	IMX8MM_PAD_ECSPI2_MOSI_ECSPI2_MOSI | MUX_PAD_CTRL(SPI_PAD_CTRL),
	IMX8MM_PAD_ECSPI2_MISO_ECSPI2_MISO | MUX_PAD_CTRL(SPI_PAD_CTRL),
	IMX8MM_PAD_ECSPI2_SS0_GPIO5_IO13 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

static void setup_spi(void)
{
	imx_iomux_v3_setup_multiple_pads(ecspi1_pads, ARRAY_SIZE(ecspi1_pads));
	imx_iomux_v3_setup_multiple_pads(ecspi2_pads, ARRAY_SIZE(ecspi2_pads));
	gpio_request(IMX_GPIO_NR(5, 9), "ECSPI1 CS");
	gpio_request(IMX_GPIO_NR(5, 13), "ECSPI2 CS");
}

int board_spi_cs_gpio(unsigned bus, unsigned cs)
{
	if (bus == 0)
		return IMX_GPIO_NR(5, 9);
	else
		return IMX_GPIO_NR(5, 13);
}
#endif

#ifdef CONFIG_NAND_MXS
#define NAND_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_FSEL2 | PAD_CTL_HYS)
#define NAND_PAD_READY0_CTRL (PAD_CTL_DSE6 | PAD_CTL_FSEL2 | PAD_CTL_PUE)
static iomux_v3_cfg_t const gpmi_pads[] = {
	IMX8MM_PAD_NAND_ALE_RAWNAND_ALE | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_CE0_B_RAWNAND_CE0_B | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_CLE_RAWNAND_CLE | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA00_RAWNAND_DATA00 | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA01_RAWNAND_DATA01 | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA02_RAWNAND_DATA02 | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA03_RAWNAND_DATA03 | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA04_RAWNAND_DATA04 | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA05_RAWNAND_DATA05	| MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA06_RAWNAND_DATA06	| MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA07_RAWNAND_DATA07	| MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_RE_B_RAWNAND_RE_B | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_READY_B_RAWNAND_READY_B | MUX_PAD_CTRL(NAND_PAD_READY0_CTRL),
	IMX8MM_PAD_NAND_WE_B_RAWNAND_WE_B | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_WP_B_RAWNAND_WP_B | MUX_PAD_CTRL(NAND_PAD_CTRL),
};

static void setup_gpmi_nand(void)
{
	imx_iomux_v3_setup_multiple_pads(gpmi_pads, ARRAY_SIZE(gpmi_pads));
}
#endif

int board_early_init_f(void)
{
	struct wdog_regs *wdog = (struct wdog_regs *)WDOG1_BASE_ADDR;

#if defined(TARGET_BUILD_VARIANT_USER) && defined(CONFIG_SILENT_CONSOLE)
	gd->flags |= GD_FLG_SILENT;
#endif

	imx_iomux_v3_setup_multiple_pads(wdog_pads, ARRAY_SIZE(wdog_pads));

	set_wdog_reset(wdog);

	imx_iomux_v3_setup_multiple_pads(uart_pads, ARRAY_SIZE(uart_pads));

#ifdef CONFIG_NAND_MXS
	setup_gpmi_nand(); /* SPL will call the board_early_init_f */
#endif

	return 0;
}

#ifdef CONFIG_BOARD_POSTCLK_INIT
int board_postclk_init(void)
{
	/* TODO */
	return 0;
}
#endif

static int get_dram_bank_count(void)
{
	uint64_t size;

#if CONFIG_NR_DRAM_BANKS > 1
	size = get_ram_size((void *)CONFIG_SYS_SDRAM_BASE,
		       (uint64_t)PHYS_SDRAM_SIZE + (uint64_t)PHYS_SDRAM_2_SIZE);
#else
	size = get_ram_size((void *)CONFIG_SYS_SDRAM_BASE,
					(uint64_t)PHYS_SDRAM_SIZE);
#endif
	if (size > (uint64_t)PHYS_SDRAM_SIZE)
		return 2;
	else
		return 1;
}

int dram_init(void)
{
	/* rom_pointer[1] contains the size of TEE occupies */
	if (rom_pointer[1])
		gd->ram_size = PHYS_SDRAM_SIZE - rom_pointer[1];
	else
		gd->ram_size = PHYS_SDRAM_SIZE;

#if CONFIG_NR_DRAM_BANKS > 1
	if (get_dram_bank_count() > 1)
		gd->ram_size += PHYS_SDRAM_2_SIZE;
#endif
	return 0;
}

int dram_init_banksize(void)
{
	gd->bd->bi_dram[0].start = PHYS_SDRAM;
	if (rom_pointer[1])
		gd->bd->bi_dram[0].size = PHYS_SDRAM_SIZE - rom_pointer[1];
	else
		gd->bd->bi_dram[0].size = PHYS_SDRAM_SIZE;

#if CONFIG_NR_DRAM_BANKS > 1
	if (get_dram_bank_count() > 1) {
		gd->bd->bi_dram[1].start = PHYS_SDRAM_2;
		gd->bd->bi_dram[1].size = PHYS_SDRAM_2_SIZE;
	}
#endif
	return 0;
}

phys_size_t get_effective_memsize(void)
{
	if (rom_pointer[1])
		return (PHYS_SDRAM_SIZE - rom_pointer[1]);
	else
		return PHYS_SDRAM_SIZE;
}

#ifdef CONFIG_OF_BOARD_SETUP
int ft_board_setup(void *blob, bd_t *bd)
{
	return 0;
}
#endif

#ifdef CONFIG_FEC_MXC
#define FEC_RST_PAD IMX_GPIO_NR(4, 22)
static iomux_v3_cfg_t const fec1_rst_pads[] = {
	IMX8MM_PAD_SAI2_RXC_GPIO4_IO22 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

static void setup_iomux_fec(void)
{
	imx_iomux_v3_setup_multiple_pads(fec1_rst_pads,
					 ARRAY_SIZE(fec1_rst_pads));

	gpio_request(FEC_RST_PAD, "fec1_rst");
	gpio_direction_output(FEC_RST_PAD, 0);
	udelay(500);
	gpio_direction_output(FEC_RST_PAD, 1);
}

static int setup_fec(void)
{
	struct iomuxc_gpr_base_regs *const iomuxc_gpr_regs
		= (struct iomuxc_gpr_base_regs *) IOMUXC_GPR_BASE_ADDR;

	setup_iomux_fec();

	/* Use 125M anatop REF_CLK1 for ENET1, not from external */
	clrsetbits_le32(&iomuxc_gpr_regs->gpr[1],
			IOMUXC_GPR_GPR1_GPR_ENET1_TX_CLK_SEL_SHIFT, 0);
	return set_clk_enet(ENET_125MHZ);
}

int board_phy_config(struct phy_device *phydev)
{
	/* enable rgmii rxc skew and phy mode select to RGMII copper */
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1d, 0x1f);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, 0x8);

	phy_write(phydev, MDIO_DEVAD_NONE, 0x1d, 0x00);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, 0x82ee);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1d, 0x05);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, 0x100);

	if (phydev->drv->config)
		phydev->drv->config(phydev);
	return 0;
}
#endif

#ifdef CONFIG_USB_TCPC
struct tcpc_port port1;
struct tcpc_port port2;

static int setup_pd_switch(uint8_t i2c_bus, uint8_t addr)
{
	struct udevice *bus;
	struct udevice *i2c_dev = NULL;
	int ret;
	uint8_t valb;

	ret = uclass_get_device_by_seq(UCLASS_I2C, i2c_bus, &bus);
	if (ret) {
		printf("%s: Can't find bus\n", __func__);
		return -EINVAL;
	}

	ret = dm_i2c_probe(bus, addr, 0, &i2c_dev);
	if (ret) {
		printf("%s: Can't find device id=0x%x\n",
			__func__, addr);
		return -ENODEV;
	}

	ret = dm_i2c_read(i2c_dev, 0xB, &valb, 1);
	if (ret) {
		printf("%s dm_i2c_read failed, err %d\n", __func__, ret);
		return -EIO;
	}
	valb |= 0x4; /* Set DB_EXIT to exit dead battery mode */
	ret = dm_i2c_write(i2c_dev, 0xB, (const uint8_t *)&valb, 1);
	if (ret) {
		printf("%s dm_i2c_write failed, err %d\n", __func__, ret);
		return -EIO;
	}

	/* Set OVP threshold to 23V */
	valb = 0x6;
	ret = dm_i2c_write(i2c_dev, 0x8, (const uint8_t *)&valb, 1);
	if (ret) {
		printf("%s dm_i2c_write failed, err %d\n", __func__, ret);
		return -EIO;
	}

	return 0;
}

int pd_switch_snk_enable(struct tcpc_port *port)
{
	if (port == &port1) {
		debug("Setup pd switch on port 1\n");
		return setup_pd_switch(1, 0x72);
	} else if (port == &port2) {
		debug("Setup pd switch on port 2\n");
		return setup_pd_switch(1, 0x73);
	} else
		return -EINVAL;
}

struct tcpc_port_config port1_config = {
	.i2c_bus = 1, /*i2c2*/
	.addr = 0x50,
	.port_type = TYPEC_PORT_UFP,
	.max_snk_mv = 5000,
	.max_snk_ma = 3000,
	.max_snk_mw = 40000,
	.op_snk_mv = 9000,
	.switch_setup_func = &pd_switch_snk_enable,
};

struct tcpc_port_config port2_config = {
	.i2c_bus = 1, /*i2c2*/
	.addr = 0x52,
	.port_type = TYPEC_PORT_UFP,
	.max_snk_mv = 5000,
	.max_snk_ma = 3000,
	.max_snk_mw = 40000,
	.op_snk_mv = 9000,
	.switch_setup_func = &pd_switch_snk_enable,
};

static int setup_typec(void)
{
	int ret;

	debug("tcpc_init port 2\n");
	ret = tcpc_init(&port2, port2_config, NULL);
	if (ret) {
		printf("%s: tcpc port2 init failed, err=%d\n",
		       __func__, ret);
	} else if (tcpc_pd_sink_check_charging(&port2)) {
		/* Disable PD for USB1, since USB2 has priority */
		port1_config.disable_pd = true;
		printf("Power supply on USB2\n");
	}

	debug("tcpc_init port 1\n");
	ret = tcpc_init(&port1, port1_config, NULL);
	if (ret) {
		printf("%s: tcpc port1 init failed, err=%d\n",
		       __func__, ret);
	} else {
		if (!port1_config.disable_pd)
			printf("Power supply on USB1\n");
		return ret;
	}

	return ret;
}

int board_usb_init(int index, enum usb_init_type init)
{
	int ret = 0;
	struct tcpc_port *port_ptr;

	debug("board_usb_init %d, type %d\n", index, init);

	if (index == 0)
		port_ptr = &port1;
	else
		port_ptr = &port2;

	imx8m_usb_power(index, true);

	if (init == USB_INIT_HOST)
		tcpc_setup_dfp_mode(port_ptr);
	else
		tcpc_setup_ufp_mode(port_ptr);

	return ret;
}

int board_usb_cleanup(int index, enum usb_init_type init)
{
	int ret = 0;

	debug("board_usb_cleanup %d, type %d\n", index, init);

	if (init == USB_INIT_HOST) {
		if (index == 0)
			ret = tcpc_disable_src_vbus(&port1);
		else
			ret = tcpc_disable_src_vbus(&port2);
	}

	imx8m_usb_power(index, false);
	return ret;
}

int board_ehci_usb_phy_mode(struct udevice *dev)
{
	int ret = 0;
	enum typec_cc_polarity pol;
	enum typec_cc_state state;
	struct tcpc_port *port_ptr;

	if (dev->seq == 0)
		port_ptr = &port1;
	else
		port_ptr = &port2;

	tcpc_setup_ufp_mode(port_ptr);

	ret = tcpc_get_cc_status(port_ptr, &pol, &state);
	if (!ret) {
		if (state == TYPEC_STATE_SRC_RD_RA || state == TYPEC_STATE_SRC_RD)
			return USB_INIT_HOST;
	}

	return USB_INIT_DEVICE;
}

#endif

#ifdef CONFIG_ICX_DEAD_BATTERY
struct udevice *sony_max1704x;
struct udevice *sony_bq25898;

#define BQ25898_I2C_ADDR 0x6B

#define DEAD_BATTERY_LOW_VOL 3350000
#define DEAD_BATTERY_CHARGER_MODE_VOL 3500000
#define DEAD_BATTERY_READ_INTERVAL 100
#define DEAD_BATTERY_SAMPLE_NUM 50

#define DEAD_BATTERY_BOOTMODE_UNKNOWN -1
#define DEAD_BATTERY_BOOTMODE_NOMAL 0
#define DEAD_BATTERY_BOOTMODE_LOW 1
#define DEAD_BATTERY_BOOTMODE_CHARGE 2

#define DEAD_BATTERY_VBUS_DET 0
#define DEAD_BATTERY_VBUS_NONE 1

#define VBUS_DET_PAD IMX_GPIO_NR(4, 22)
static iomux_v3_cfg_t const vbus_det_lv_pads[] = {
	IMX8MM_PAD_SAI2_RXC_GPIO4_IO22 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

static int bq25898_i2c_reg_write(struct udevice *dev, uint addr, uint mask, uint data)
{
	uint8_t valb;
	int err;

	valb = data & mask;
	err = dm_i2c_write(dev, addr, &valb, 1);
	return err;
}

static void set_bq25898_current(struct udevice *dev,int current)
{
	if (dev == NULL) {
		printf("[Battery] invalid udevice\n");
		return;
	}
	debug("[Battery]%s: current:%d\n", __func__, current);

	if (current == 500) {
		printf("[Battery] force 500mA setting.\n");
		bq25898_i2c_reg_write(dev, 0x00, 0xff, 0x48);//500mA
	}
	else {
		bq25898_i2c_reg_write(dev, 0x00, 0xff, 0x40);//100mA
	}
}

static int dead_battery_get_bootmode(void)
{
	int voltage;
	int vbus_det;
	int ret;
	static int boot_mode = DEAD_BATTERY_BOOTMODE_UNKNOWN;
	static int voltage_cache = 0;

	ret = max1704x_get_battery_voltage(sony_max1704x, &voltage);
	if (ret < 0) {
		printf("[Battery] Error read voltage\n");
		return DEAD_BATTERY_BOOTMODE_NOMAL;
	}

	printf("[battery] voltage %d \n",voltage);
	if (boot_mode != DEAD_BATTERY_BOOTMODE_UNKNOWN) {
		printf("[battery] initial voltage %d \n",voltage_cache);
		env_set_ulong("battery_voltage", (unsigned long)voltage_cache);
		return boot_mode;
	}
	env_set_ulong("battery_voltage", (unsigned long)voltage);
	voltage_cache = voltage;

	vbus_det = gpio_get_value(VBUS_DET_PAD);
	if (vbus_det == DEAD_BATTERY_VBUS_NONE) {
		if (voltage < DEAD_BATTERY_LOW_VOL)
			boot_mode =  DEAD_BATTERY_BOOTMODE_LOW;
		else
			boot_mode =  DEAD_BATTERY_BOOTMODE_NOMAL;
	}
	else { //VBUS_DET
		if (voltage < DEAD_BATTERY_CHARGER_MODE_VOL)
			boot_mode =  DEAD_BATTERY_BOOTMODE_CHARGE;
		else
			boot_mode =  DEAD_BATTERY_BOOTMODE_NOMAL;
	}

	return boot_mode;
}

static void setup_dead_battery(void)
{
	imx_iomux_v3_setup_multiple_pads(vbus_det_lv_pads,
				 ARRAY_SIZE(vbus_det_lv_pads));
	gpio_request(VBUS_DET_PAD, "VBUS_DET");
	gpio_direction_input(VBUS_DET_PAD);
}

static struct udevice* setup_bq25898(void)
{
	struct udevice *bus, *bq25898_dev;
	int i2c_bus = 0;
	int ret;
	int mode;

	ret = uclass_get_device_by_seq(UCLASS_I2C, i2c_bus, &bus);
	if (ret) {
		printf("%s: No bus %d\n", __func__, i2c_bus);
		return NULL;
	}
	ret = dm_i2c_probe(bus, BQ25898_I2C_ADDR, 0, &bq25898_dev);
	if (ret) {
		printf("%s: Can't find device id=0x%x, on bus %d\n",
			__func__, BQ25898_I2C_ADDR, i2c_bus);
		return NULL;
	}

	bq25898_i2c_reg_write(bq25898_dev, 0x02, 0xff, 0x00);
	bq25898_i2c_reg_write(bq25898_dev, 0x03, 0xff, 0x34);
	bq25898_i2c_reg_write(bq25898_dev, 0x04, 0xff, 0x0F);
	bq25898_i2c_reg_write(bq25898_dev, 0x05, 0xff, 0x10);
	bq25898_i2c_reg_write(bq25898_dev, 0x06, 0xff, 0x56);
	bq25898_i2c_reg_write(bq25898_dev, 0x07, 0xff, 0x8A);
	bq25898_i2c_reg_write(bq25898_dev, 0x09, 0xff, 0x44);
	bq25898_i2c_reg_write(bq25898_dev, 0x0A, 0xff, 0x80);
	bq25898_i2c_reg_write(bq25898_dev, 0x0D, 0xff, 0x92);

	mode = dead_battery_get_bootmode();
	if (mode == DEAD_BATTERY_BOOTMODE_LOW
	      || mode == DEAD_BATTERY_BOOTMODE_CHARGE) {
		set_bq25898_current(bq25898_dev, 500);
	}
	else {
		set_bq25898_current(bq25898_dev, 100);
	}

	return bq25898_dev;
}

static int dead_battery_shutdown_wait_vbus(void)
{
	int i,vbus_det;
	for (i = 1; i <= DEAD_BATTERY_SAMPLE_NUM; ++i) {
		if (i == DEAD_BATTERY_SAMPLE_NUM)
			snvs_poweroff();
		vbus_det = gpio_get_value(VBUS_DET_PAD);
		if (vbus_det == DEAD_BATTERY_VBUS_DET) {
			printf("[Battery] Connect V-bus. Continue boot\n");
			break;
		}
		mdelay(DEAD_BATTERY_READ_INTERVAL);
	}
	return 0;
}
#endif

#if (defined(CONFIG_ICX_OTG_ALL_SUSPEND))

#define USB1_nPORTSC1		(0x32e40184)
#define USBNC_1_PHY_CFG2	(0x32e40234)
#define USB2_nPORTSC1		(0x32e50184)
#define USBNC_2_PHY_CFG2	(0x32e50234)

#define USBx_nPORTSC1_PHCD	  (0x800000)
#define USBNC_n_PHY_CFG2_DRVVBUS0  (0x10000)

static void board_init_otg_all_suspend(void)
{
	void __iomem *_umem;
	u32 val;

	enable_usboh3_clk(1);

	/* OTG1: Disable PHY clock */
	_umem = (void __iomem *)USB1_nPORTSC1;
	val = readl(_umem);
	val |= USBx_nPORTSC1_PHCD;
	writel(val, _umem);
	/* OTG1: Disable VBUS comparator */
	_umem = (void __iomem *)USBNC_1_PHY_CFG2;
	val = readl(_umem);
	val &= ~USBNC_n_PHY_CFG2_DRVVBUS0;
	writel(val, _umem);

	/* OTG2: Disable PHY clock */
	_umem = (void __iomem *)USB2_nPORTSC1;
	val = readl(_umem);
	val |= USBx_nPORTSC1_PHCD;
	writel(val, _umem);
	/* OTG2: Disable VBUS comparator */
	_umem = (void __iomem *)USBNC_2_PHY_CFG2;
	val = readl(_umem);
	val &= ~USBNC_n_PHY_CFG2_DRVVBUS0;
	writel(val, _umem);

	enable_usboh3_clk(0);
}
#endif /* (defined(CONFIG_ICX_OTG_ALL_SUSPEND)) */

#define I2C_PMIC 0
#define BD71847	0x4b
static int power_bd71847_setting_override(void)
{
	struct udevice *bus, *dev;
	int i2c_bus = I2C_PMIC;
	int ret;
	uint8_t val;

	ret = uclass_get_device_by_seq(UCLASS_I2C, i2c_bus, &bus);
	if (ret) {
		printf("%s: No bus %d\n", __func__, i2c_bus);
		return -EINVAL;
	}

	ret = dm_i2c_probe(bus, BD71847, 0, &dev);
	if (ret) {
		printf("%s: Can't find device id=0x%x, on bus %d\n",
			__func__, BD71847, i2c_bus);
		return -ENODEV;
	}

	ret = dm_i2c_read(dev, 0x0, &val, 1);
	if (ret) {
		printf("%s: ERR reading PMIC reg\n", __func__);
		return -EIO;
	}

	/* unlock the PMIC regs */
	ret = dm_i2c_read(dev, BD71837_REGLOCK, &val, 1);
	if (ret) {
		printf("%s: ERR reading BD71837_REGLOCK reg\n", __func__);
		return -EIO;
	}

	val &= 0xef;

	ret = dm_i2c_write(dev, BD71837_REGLOCK, &val, 1);
	if (ret) {
		printf("%s: ERR writing BD71837_REGLOCK reg\n", __func__);
		return -EIO;
	}

	/* increase NVCC_DRAM BUCK8 from 1.12V to 1.15v using 10mV steps */
	for (val=0x21; val<0x24; val++) {
		ret = dm_i2c_write(dev, BD71837_BUCK8_VOLT, &val, 1);
		if (ret) {
			printf("%s: ERR writing BD71837_BUCK8_VOLT reg\n", __func__);
			return -EIO;
		}
		else
			debug("%s: Updated BUCK8 to 0x%x\n", __func__, val);
	}


	/* relock the PMIC regs */
	ret = dm_i2c_read(dev, BD71837_REGLOCK, &val, 1);
	if (ret) {
		printf("%s: ERR reading BD71837_REGLOCK reg\n", __func__);
		return -EIO;
	}

	val |= 0x10;

	ret = dm_i2c_write(dev, BD71837_REGLOCK, &val, 1);
	if (ret) {
		printf("%s: ERR writing BD71837_REGLOCK reg\n", __func__);
		return -EIO;
	}

	/* Read RCVCFG regs */
	ret = dm_i2c_read(dev, BD71837_RCVCFG, &val, 1);
	if (ret) {
		printf("%s: ERR reading BD71837_RCVCFG reg\n", __func__);
		return -EIO;
	}

	/* Change RCVCFG regs for No limit of attempts to recover */
	val |= 0xf0;

	/* Write RCVCFG regs */
	ret = dm_i2c_write(dev, BD71837_RCVCFG, &val, 1);
	if (ret) {
		printf("%s: ERR writing BD71837_RCVCFG reg\n", __func__);
		return -EIO;
	}

	/* Read Recovery Number Register */
	ret = dm_i2c_read(dev, BD71837_RCVNUM, &val, 1);
	if (ret) {
		printf("%s: ERR reading BD71837_RCVNUM reg\n", __func__);
		return -EIO;
	}
	printf("[pmic] Recovery Number Register:%d\n",val);

	return 0;
}

int board_init(void)
{

/*
 * If you need to override pmic settings which are defined in spl,
 * here is the recommended place to do.
*/
	power_bd71847_setting_override();

#if defined(CONFIG_ICX_PORT_INIT)
	initr_icx_port_init();
#endif /* defined(CONFIG_ICX_PORT_INIT) */

#if defined(CONFIG_ICX_DMP_BOARD_ID)
	initr_icx_dmp_board_id();
#endif /* defined(CONFIG_ICX_DMP_BOARD_ID) */

#if defined(CONFIG_MAX1704X)
	sony_max1704x = initr_max1704x();
#endif /* defined(CONFIG_MAX1704X) */

#ifdef CONFIG_ICX_DEAD_BATTERY
	setup_dead_battery();
	sony_bq25898 = setup_bq25898();
#endif

#ifdef CONFIG_USB_TCPC
	setup_typec();
#endif

#ifdef CONFIG_MXC_SPI
	setup_spi();
#endif

#ifdef CONFIG_FEC_MXC
	setup_fec();
#endif

#ifdef CONFIG_FSL_FSPI
	board_qspi_init();
#endif

#if (defined(CONFIG_ICX_OTG_ALL_SUSPEND))
	board_init_otg_all_suspend();
#endif /* (defined(CONFIG_ICX_OTG_ALL_SUSPEND)) */

	return 0;
}

int board_mmc_get_env_dev(int devno)
{
	return devno - 1;
}

int mmc_map_to_kernel_blk(int devno)
{
	return devno + 1;
}

#ifdef CONFIG_VIDEO_MXS

#define ADV7535_MAIN 0x3d
#define ADV7535_DSI_CEC 0x3c

static const struct sec_mipi_dsim_plat_data imx8mm_mipi_dsim_plat_data = {
	.version	= 0x1060200,
	.max_data_lanes = 4,
	.max_data_rate  = 1500000000ULL,
	.reg_base = MIPI_DSI_BASE_ADDR,
	.gpr_base = CSI_BASE_ADDR + 0x8000,
};

static int adv7535_i2c_reg_write(struct udevice *dev, uint addr, uint mask, uint data)
{
	uint8_t valb;
	int err;

	if (mask != 0xff) {
		err = dm_i2c_read(dev, addr, &valb, 1);
		if (err)
			return err;

		valb &= ~mask;
		valb |= data;
	} else {
		valb = data;
	}

	err = dm_i2c_write(dev, addr, &valb, 1);
	return err;
}

static int adv7535_i2c_reg_read(struct udevice *dev, uint8_t addr, uint8_t *data)
{
	uint8_t valb;
	int err;

	err = dm_i2c_read(dev, addr, &valb, 1);
	if (err)
		return err;

	*data = (int)valb;
	return 0;
}

static void adv7535_init(void)
{
	struct udevice *bus, *main_dev, *cec_dev;
	int i2c_bus = 1;
	int ret;
	uint8_t val;

	ret = uclass_get_device_by_seq(UCLASS_I2C, i2c_bus, &bus);
	if (ret) {
		printf("%s: No bus %d\n", __func__, i2c_bus);
		return;
	}

	ret = dm_i2c_probe(bus, ADV7535_MAIN, 0, &main_dev);
	if (ret) {
		printf("%s: Can't find device id=0x%x, on bus %d\n",
			__func__, ADV7535_MAIN, i2c_bus);
		return;
	}

	ret = dm_i2c_probe(bus, ADV7535_DSI_CEC, 0, &cec_dev);
	if (ret) {
		printf("%s: Can't find device id=0x%x, on bus %d\n",
			__func__, ADV7535_MAIN, i2c_bus);
		return;
	}

	adv7535_i2c_reg_read(main_dev, 0x00, &val);
	debug("Chip revision: 0x%x (expected: 0x14)\n", val);
	adv7535_i2c_reg_read(cec_dev, 0x00, &val);
	debug("Chip ID MSB: 0x%x (expected: 0x75)\n", val);
	adv7535_i2c_reg_read(cec_dev, 0x01, &val);
	debug("Chip ID LSB: 0x%x (expected: 0x33)\n", val);

	/* Power */
	adv7535_i2c_reg_write(main_dev, 0x41, 0xff, 0x10);
	/* Initialisation (Fixed) Registers */
	adv7535_i2c_reg_write(main_dev, 0x16, 0xff, 0x20);
	adv7535_i2c_reg_write(main_dev, 0x9A, 0xff, 0xE0);
	adv7535_i2c_reg_write(main_dev, 0xBA, 0xff, 0x70);
	adv7535_i2c_reg_write(main_dev, 0xDE, 0xff, 0x82);
	adv7535_i2c_reg_write(main_dev, 0xE4, 0xff, 0x40);
	adv7535_i2c_reg_write(main_dev, 0xE5, 0xff, 0x80);
	adv7535_i2c_reg_write(cec_dev, 0x15, 0xff, 0xD0);
	adv7535_i2c_reg_write(cec_dev, 0x17, 0xff, 0xD0);
	adv7535_i2c_reg_write(cec_dev, 0x24, 0xff, 0x20);
	adv7535_i2c_reg_write(cec_dev, 0x57, 0xff, 0x11);
	/* 4 x DSI Lanes */
	adv7535_i2c_reg_write(cec_dev, 0x1C, 0xff, 0x40);

	/* DSI Pixel Clock Divider */
	adv7535_i2c_reg_write(cec_dev, 0x16, 0xff, 0x18);

	/* Enable Internal Timing Generator */
	adv7535_i2c_reg_write(cec_dev, 0x27, 0xff, 0xCB);
	/* 1920 x 1080p 60Hz */
	adv7535_i2c_reg_write(cec_dev, 0x28, 0xff, 0x89); /* total width */
	adv7535_i2c_reg_write(cec_dev, 0x29, 0xff, 0x80); /* total width */
	adv7535_i2c_reg_write(cec_dev, 0x2A, 0xff, 0x02); /* hsync */
	adv7535_i2c_reg_write(cec_dev, 0x2B, 0xff, 0xC0); /* hsync */
	adv7535_i2c_reg_write(cec_dev, 0x2C, 0xff, 0x05); /* hfp */
	adv7535_i2c_reg_write(cec_dev, 0x2D, 0xff, 0x80); /* hfp */
	adv7535_i2c_reg_write(cec_dev, 0x2E, 0xff, 0x09); /* hbp */
	adv7535_i2c_reg_write(cec_dev, 0x2F, 0xff, 0x40); /* hbp */

	adv7535_i2c_reg_write(cec_dev, 0x30, 0xff, 0x46); /* total height */
	adv7535_i2c_reg_write(cec_dev, 0x31, 0xff, 0x50); /* total height */
	adv7535_i2c_reg_write(cec_dev, 0x32, 0xff, 0x00); /* vsync */
	adv7535_i2c_reg_write(cec_dev, 0x33, 0xff, 0x50); /* vsync */
	adv7535_i2c_reg_write(cec_dev, 0x34, 0xff, 0x00); /* vfp */
	adv7535_i2c_reg_write(cec_dev, 0x35, 0xff, 0x40); /* vfp */
	adv7535_i2c_reg_write(cec_dev, 0x36, 0xff, 0x02); /* vbp */
	adv7535_i2c_reg_write(cec_dev, 0x37, 0xff, 0x40); /* vbp */

	/* Reset Internal Timing Generator */
	adv7535_i2c_reg_write(cec_dev, 0x27, 0xff, 0xCB);
	adv7535_i2c_reg_write(cec_dev, 0x27, 0xff, 0x8B);
	adv7535_i2c_reg_write(cec_dev, 0x27, 0xff, 0xCB);

	/* HDMI Output */
	adv7535_i2c_reg_write(main_dev, 0xAF, 0xff, 0x16);
	/* AVI Infoframe - RGB - 16-9 Aspect Ratio */
	adv7535_i2c_reg_write(main_dev, 0x55, 0xff, 0x02);
	adv7535_i2c_reg_write(main_dev, 0x56, 0xff, 0x0);

	/*  GC Packet Enable */
	adv7535_i2c_reg_write(main_dev, 0x40, 0xff, 0x0);
	/*  GC Colour Depth - 24 Bit */
	adv7535_i2c_reg_write(main_dev, 0x4C, 0xff, 0x0);
	/*  Down Dither Output Colour Depth - 8 Bit (default) */
	adv7535_i2c_reg_write(main_dev, 0x49, 0xff, 0x00);

	/* set low refresh 1080p30 */
	adv7535_i2c_reg_write(main_dev, 0x4A, 0xff, 0x80); /*should be 0x80 for 1080p60 and 0x8c for 1080p30*/

	/* HDMI Output Enable */
	adv7535_i2c_reg_write(cec_dev, 0xbe, 0xff, 0x3c);
	adv7535_i2c_reg_write(cec_dev, 0x03, 0xff, 0x89);
}

#define DISPLAY_MIX_SFT_RSTN_CSR		0x00
#define DISPLAY_MIX_CLK_EN_CSR		0x04

   /* 'DISP_MIX_SFT_RSTN_CSR' bit fields */
#define BUS_RSTN_BLK_SYNC_SFT_EN	BIT(6)

   /* 'DISP_MIX_CLK_EN_CSR' bit fields */
#define LCDIF_PIXEL_CLK_SFT_EN		BIT(7)
#define LCDIF_APB_CLK_SFT_EN		BIT(6)

void disp_mix_bus_rstn_reset(ulong gpr_base, bool reset)
{
	if (!reset)
		/* release reset */
		setbits_le32(gpr_base + DISPLAY_MIX_SFT_RSTN_CSR, BUS_RSTN_BLK_SYNC_SFT_EN);
	else
		/* hold reset */
		clrbits_le32(gpr_base + DISPLAY_MIX_SFT_RSTN_CSR, BUS_RSTN_BLK_SYNC_SFT_EN);
}

void disp_mix_lcdif_clks_enable(ulong gpr_base, bool enable)
{
	if (enable)
		/* enable lcdif clks */
		setbits_le32(gpr_base + DISPLAY_MIX_CLK_EN_CSR, LCDIF_PIXEL_CLK_SFT_EN | LCDIF_APB_CLK_SFT_EN);
	else
		/* disable lcdif clks */
		clrbits_le32(gpr_base + DISPLAY_MIX_CLK_EN_CSR, LCDIF_PIXEL_CLK_SFT_EN | LCDIF_APB_CLK_SFT_EN);
}

struct mipi_dsi_client_dev adv7535_dev = {
	.channel	= 0,
	.lanes = 4,
	.format  = MIPI_DSI_FMT_RGB888,
	.mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
			  MIPI_DSI_MODE_EOT_PACKET | MIPI_DSI_MODE_VIDEO_HSE,
	.name = "ADV7535",
};

struct mipi_dsi_client_dev rm67191_dev = {
	.channel	= 0,
	.lanes = 4,
	.format  = MIPI_DSI_FMT_RGB888,
	.mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
			  MIPI_DSI_MODE_EOT_PACKET | MIPI_DSI_MODE_VIDEO_HSE,
};

struct mipi_dsi_client_dev hx8379c_dev = {
	.channel	= 0,
	.lanes = 2,
	.format  = MIPI_DSI_FMT_RGB888,
	.mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
			MIPI_DSI_MODE_EOT_PACKET | MIPI_DSI_MODE_VIDEO_HSE,
};

struct mipi_dsi_client_dev hx83102d_dev = {
	.channel	= 0,
	.lanes = 2,
	.format  = MIPI_DSI_FMT_RGB888,
	.mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
			  MIPI_DSI_MODE_EOT_PACKET | MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_CLOCK_NON_CONTINUOUS,
};

#define FSL_SIP_GPC			0xC2000000
#define FSL_SIP_CONFIG_GPC_PM_DOMAIN	0x3
#define DISPMIX				9
#define MIPI				10

void do_enable_mipi2hdmi(struct display_info_t const *dev)
{
	gpio_request(IMX_GPIO_NR(1, 8), "DSI EN");
	gpio_direction_output(IMX_GPIO_NR(1, 8), 1);

	/* ADV7353 initialization */
	adv7535_init();

	/* enable the dispmix & mipi phy power domain */
	call_imx_sip(FSL_SIP_GPC, FSL_SIP_CONFIG_GPC_PM_DOMAIN, DISPMIX, true, 0);
	call_imx_sip(FSL_SIP_GPC, FSL_SIP_CONFIG_GPC_PM_DOMAIN, MIPI, true, 0);

	/* Put lcdif out of reset */
	disp_mix_bus_rstn_reset(imx8mm_mipi_dsim_plat_data.gpr_base, false);
	disp_mix_lcdif_clks_enable(imx8mm_mipi_dsim_plat_data.gpr_base, true);

	/* Setup mipi dsim */
	sec_mipi_dsim_setup(&imx8mm_mipi_dsim_plat_data);
	imx_mipi_dsi_bridge_attach(&adv7535_dev); /* attach adv7535 device */
}

void do_enable_mipi_led(struct display_info_t const *dev)
{
	gpio_request(IMX_GPIO_NR(1, 8), "DSI EN");
	gpio_direction_output(IMX_GPIO_NR(1, 8), 0);
	mdelay(100);
	gpio_direction_output(IMX_GPIO_NR(1, 8), 1);

	/* enable the dispmix & mipi phy power domain */
	call_imx_sip(FSL_SIP_GPC, FSL_SIP_CONFIG_GPC_PM_DOMAIN, DISPMIX, true, 0);
	call_imx_sip(FSL_SIP_GPC, FSL_SIP_CONFIG_GPC_PM_DOMAIN, MIPI, true, 0);

	/* Put lcdif out of reset */
	disp_mix_bus_rstn_reset(imx8mm_mipi_dsim_plat_data.gpr_base, false);
	disp_mix_lcdif_clks_enable(imx8mm_mipi_dsim_plat_data.gpr_base, true);

	/* Setup mipi dsim */
	sec_mipi_dsim_setup(&imx8mm_mipi_dsim_plat_data);

	rm67191_init();
	rm67191_dev.name = displays[1].mode.name;
	imx_mipi_dsi_bridge_attach(&rm67191_dev); /* attach rm67191 device */
}

void do_enable_mipi_himax_8379c(struct display_info_t const *dev)
{
	gpio_request(IMX_GPIO_NR(1, 1), "backlight_en");
	gpio_direction_output(IMX_GPIO_NR(1, 1), 1);

	gpio_request(IMX_GPIO_NR(1, 8), "DSI EN");
	gpio_direction_output(IMX_GPIO_NR(1, 8), 1);
	mdelay(20);
	gpio_direction_output(IMX_GPIO_NR(1, 8), 0);
	mdelay(20);
	gpio_direction_output(IMX_GPIO_NR(1, 8), 1);
	mdelay(20);

	/* enable the dispmix & mipi phy power domain */
	call_imx_sip(FSL_SIP_GPC, FSL_SIP_CONFIG_GPC_PM_DOMAIN, DISPMIX, true, 0);
	call_imx_sip(FSL_SIP_GPC, FSL_SIP_CONFIG_GPC_PM_DOMAIN, MIPI, true, 0);

	/* Put lcdif out of reset */
	disp_mix_bus_rstn_reset(imx8mm_mipi_dsim_plat_data.gpr_base, false);
	disp_mix_lcdif_clks_enable(imx8mm_mipi_dsim_plat_data.gpr_base, true);

	/* Setup mipi dsim */
	sec_mipi_dsim_setup(&imx8mm_mipi_dsim_plat_data);

	hx8379c_init();
	hx8379c_dev.name = displays[2].mode.name;
	imx_mipi_dsi_bridge_attach(&hx8379c_dev); /* attach hx8379c device */
}

static iomux_v3_cfg_t const lcd_pads[] = {
	IMX8MM_PAD_ENET_RD3_GPIO1_IO29 | MUX_PAD_CTRL(NO_PAD_CTRL),
	IMX8MM_PAD_SD1_STROBE_GPIO2_IO11 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

#define TPS65132_I2C_ADDR 0x3e
#define TPS65132_VPOS_SETTING 0x0f
#define TPS65132_VNEG_SETTING 0x0f
#define LCD_XRST_CONTROL_REGISTER (IOMUXC_BASE_ADDR + 0x2B0)

static void pwm_backlight_on(void)
{
	gpio_request(IMX_GPIO_NR(1, 1), "backlight_en");
	gpio_direction_output(IMX_GPIO_NR(1, 1), 1);
}

void do_enable_mipi_himax_83102d(struct display_info_t const *dev)
{
	struct udevice *bus, *tps65132_dev;
	int i2c_bus = 0;
	int ret;
	uint8_t vpos,vneg;
	uint8_t valb;
	uint32_t lcd_rst_val;

	ret = uclass_get_device_by_seq(UCLASS_I2C, i2c_bus, &bus);
	if (ret) {
		printf("%s: No bus %d\n", __func__, i2c_bus);
		return;
	}

	imx_iomux_v3_setup_multiple_pads(lcd_pads, ARRAY_SIZE(lcd_pads));
	gpio_request(IMX_GPIO_NR(1, 29), "LCD_PWRP_EN");
	gpio_direction_output(IMX_GPIO_NR(1, 29), 1);
	mdelay(5);
	gpio_request(IMX_GPIO_NR(2, 11), "LCD_PWRN_EN");
	gpio_direction_output(IMX_GPIO_NR(2, 11), 1);
	mdelay(5);

	ret = dm_i2c_probe(bus, TPS65132_I2C_ADDR, 0, &tps65132_dev);
	if (ret) {
		printf("%s: Can't find device id=0x%x, on bus %d\n",
			__func__, TPS65132_I2C_ADDR, i2c_bus);
		return;
	}
	dm_i2c_read(tps65132_dev, 0x00, &vpos, 1);
	dm_i2c_read(tps65132_dev, 0x01, &vneg, 1);
	if (vpos != TPS65132_VPOS_SETTING || vneg != TPS65132_VNEG_SETTING) {
		printf("[ERR]TPS65132 invalid setting 0x00:0x%x 0x01:0x%x\n"
								,vpos,vneg);
		valb = TPS65132_VPOS_SETTING;
		dm_i2c_write(tps65132_dev, 0x00, &valb,1);
		valb = TPS65132_VNEG_SETTING;
		dm_i2c_write(tps65132_dev, 0x01, &valb,1);
		/* flash eeprom */
		valb = 0x80;
		dm_i2c_write(tps65132_dev, 0xff, &valb,1);
	}

	lcd_rst_val = readl(LCD_XRST_CONTROL_REGISTER);
	lcd_rst_val |= (1<<6);
	writel(lcd_rst_val, LCD_XRST_CONTROL_REGISTER);

	/* enable the dispmix & mipi phy power domain */
	call_imx_sip(FSL_SIP_GPC, FSL_SIP_CONFIG_GPC_PM_DOMAIN, DISPMIX, true, 0);
	call_imx_sip(FSL_SIP_GPC, FSL_SIP_CONFIG_GPC_PM_DOMAIN, MIPI, true, 0);

	/* Put lcdif out of reset */
	disp_mix_bus_rstn_reset(imx8mm_mipi_dsim_plat_data.gpr_base, false);
	disp_mix_lcdif_clks_enable(imx8mm_mipi_dsim_plat_data.gpr_base, true);

	/* Setup mipi dsim */
	sec_mipi_dsim_setup(&imx8mm_mipi_dsim_plat_data);

	hx83102d_init();
	hx83102d_dev.name = displays[3].mode.name;
	imx_mipi_dsi_bridge_attach(&hx83102d_dev); /* attach hx83102d device */
#ifdef CONFIG_ICX_DEAD_BATTERY
	switch (dead_battery_get_bootmode()) {
	case DEAD_BATTERY_BOOTMODE_LOW :
		env_set("boot_logo", "icx_low_battery.bmp.gz");
		env_set("kernel_logo", "icx_low_battery.bmp.gz");
		break;
	case DEAD_BATTERY_BOOTMODE_CHARGE :
		env_set("boot_logo", "icx_pre_charge.bmp.gz");
		env_set("kernel_logo", "icx_pre_charge.bmp.gz");
		break;
	default :
		break;
	}
#endif

}
void board_quiesce_devices(void)
{
	//gpio_request(IMX_GPIO_NR(1, 8), "DSI EN");
	//gpio_direction_output(IMX_GPIO_NR(1, 8), 0);
}

struct display_info_t const displays[] = {{
	.bus = LCDIF_BASE_ADDR,
	.addr = 0,
	.pixfmt = 24,
	.detect = NULL,
	.enable	= do_enable_mipi2hdmi,
	.mode	= {
		.name			= "MIPI2HDMI",
		.refresh		= 60,
		.xres			= 1920,
		.yres			= 1080,
		.pixclock		= 6734, /* 148500000 */
		.left_margin	= 148,
		.right_margin	= 88,
		.upper_margin	= 36,
		.lower_margin	= 4,
		.hsync_len		= 44,
		.vsync_len		= 5,
		.sync			= FB_SYNC_EXT,
		.vmode			= FB_VMODE_NONINTERLACED

} }, {
	.bus = LCDIF_BASE_ADDR,
	.addr = 0,
	.pixfmt = 24,
	.detect = NULL,
	.enable	= do_enable_mipi_led,
	.mode	= {
		.name			= "RM67191_OLED",
		.refresh		= 60,
		.xres			= 1080,
		.yres			= 1920,
		.pixclock		= 7575, /* 132000000 */
		.left_margin	= 34,
		.right_margin	= 20,
		.upper_margin	= 4,
		.lower_margin	= 10,
		.hsync_len		= 2,
		.vsync_len		= 2,
		.sync			= FB_SYNC_EXT,
		.vmode			= FB_VMODE_NONINTERLACED

 } }, {
	.bus		= LCDIF_BASE_ADDR,
	.addr		= 0,
	.pixfmt		= 24,
	.detect		= NULL,
	.enable		= do_enable_mipi_himax_8379c,
	.mode		= {
		.name		= "HX8379C_LCD",
		.refresh	= 60,
		.xres		= 480,
		.yres		= 800,
		.pixclock	= 33333, /* 33000000 */
		.left_margin	= 35,
		.right_margin	= 35,
		.upper_margin	= 10,
		.lower_margin	= 5,
		.hsync_len	= 37,
		.vsync_len	= 2,
		.sync		= FB_SYNC_EXT,
		.vmode		= FB_VMODE_NONINTERLACED

 } }, {
	.bus = LCDIF_BASE_ADDR,
	.addr = 0,
	.pixfmt = 24,
	.detect = NULL,
	.enable	= do_enable_mipi_himax_83102d,
	.mode	= {
		.name			= "HX83102D_LCD",
		.refresh		= 60,
		.xres			= 720,
		.yres			= 1280,
		.pixclock		= 15151, /* 66000000 */
		.left_margin		= 10,
		.right_margin		= 20,
		.upper_margin		= 14,
		.lower_margin		= 146,
		.hsync_len		= 12,
		.vsync_len		= 2,
		.sync			= FB_SYNC_EXT,
		.vmode			= FB_VMODE_NONINTERLACED

 } }
};
size_t display_count = ARRAY_SIZE(displays);
#endif

int board_late_init(void)
{
#ifdef CONFIG_VIDEO_MXS
	pwm_backlight_on();
#endif

#ifdef CONFIG_ENV_IS_IN_MMC
	board_late_mmc_env_init();
#endif

	/* enable DDRC PWRCTL: en_dfi_dram_clk_disable, powerdown_en, selfref_en */
	writel(readl(DDRC_PWRCTL(0)) | 0xb, DDRC_PWRCTL(0));

	(void) icx_nvp_init();
#if !defined(TARGET_BUILD_VARIANT_USER) && defined(CONFIG_SILENT_CONSOLE)
	if (gd->flags & GD_FLG_ENV_DEFAULT)
		env_set("silent", "1");
#endif
#ifdef CONFIG_SILENT_CONSOLE
	if (env_get("silent") && !nvp_emmc_check_printk())
		gd->flags |= GD_FLG_SILENT;
	else
		gd->flags &= ~GD_FLG_SILENT;
#endif

#ifdef CONFIG_ICX_DEAD_BATTERY
	if (dead_battery_get_bootmode() ==  DEAD_BATTERY_BOOTMODE_LOW) {
		printf("[Battery] Low battery. Will shutdown\n");
		dead_battery_shutdown_wait_vbus();
	}
	if (dead_battery_get_bootmode() == DEAD_BATTERY_BOOTMODE_CHARGE
	    || dead_battery_get_bootmode() ==  DEAD_BATTERY_BOOTMODE_LOW) {
		printf("[Battery] Goto charger mode\n");
		set_bq25898_current(sony_bq25898, 500);
		env_set("bootmode", "charger");
	}
#endif
	return 0;
}

void board_disable_console(void)
{
	imx_iomux_v3_setup_multiple_pads(console_gpio_pads,
					 ARRAY_SIZE(console_gpio_pads));

	gpio_request(CONSOLE_RXD_GPIO, "CONSOLE_RXD_GPIO");
	gpio_direction_output(CONSOLE_RXD_GPIO, 1);
	gpio_set_value(CONSOLE_RXD_GPIO, 1);

	gpio_request(CONSOLE_TXD_GPIO, "CONSOLE_TXD_GPIO");
	gpio_direction_output(CONSOLE_TXD_GPIO, 1);
	gpio_set_value(CONSOLE_TXD_GPIO, 1);
}

#ifdef CONFIG_FSL_FASTBOOT
#define KEY_ENOUGH_TIME_MS	(100)
#define KEY_PRESS_MAX_TIME_MS	(8000)

#if (defined(CONFIG_ANDROID_RECOVERY) \
     || defined(CONFIG_ICX_FASTBOOT_MAGIC_KEY))
int icx_key_pressing_probe(void)
{
	printf("recovery: [VOL-]\n"
	       "fastboot: [FF]\n"
	);
	return 0; /* Anyway continue. */
}

static unsigned long long tick_to_ms(uint64_t tick)
{	unsigned long tb = get_tbclk();

	tick *= 1000;
	do_div(tick, tb);
	return tick;
}

#define	RECOVERY_KEY_STATE_START	(0)
#define	RECOVERY_KEY_STATE_ACCEPT	(1)
#define	RECOVERY_KEY_STATE_TOO_LONG	(2)


#endif /* CONFIG_ANDROID_RECOVERY || CONFIG_ICX_FASTBOOT_MAGIC_KEY */
#ifdef CONFIG_ANDROID_RECOVERY
#if (defined(CONFIG_SUPPORT_SERVICE_MODE) && defined(TARGET_BUILD_VARIANT_USERDEBUG))
int is_recovery_service_mode(void)
{
	int ret = 1;
	int mode = icx_nvp_get_bootmode();

	printf("%s: Boot mode status. mode=%d\n", __func__, mode);

	switch (mode) {
	case NVP_BOOTMODE_INIT :
		env_set("bootmode", "init");
		break;
	case NVP_BOOTMODE_DIAG :
		env_set("bootmode", "diag");
		break;
	default : /* NVP_BOOTMODE_NORMAL */
		ret = 0;
		break;
	}

	return ret;
}
#endif /*CONFIG_SUPPORT_SERVICE_MODE*/

int is_recovery_key_pressing(void)
{
	int	kfr = -1;
	int	state = RECOVERY_KEY_STATE_START;

	uint64_t		tick_base = 0;
	uint64_t		tick_elapsed = 0;
	unsigned long long	press_ms = 0;

	tick_base = get_ticks();

	while ((press_ms = tick_to_ms(tick_elapsed))
		<= KEY_PRESS_MAX_TIME_MS) {

		kfr = gpio_get_value(ICX_DMP_KEY_VOL_MINUS);
		if (kfr != ICX_DMP_KEY_PRESSED)
			break;

		switch (state) {
		case RECOVERY_KEY_STATE_START:
			if (press_ms >= KEY_ENOUGH_TIME_MS) {
				printf("%s: Accept recovery, Release Key.\n",
					__func__
				);
				state = RECOVERY_KEY_STATE_ACCEPT;
			}
			break;
		case RECOVERY_KEY_STATE_ACCEPT:
			if (press_ms >= KEY_PRESS_MAX_TIME_MS) {
				printf("%s: Pressing key too long, Normal boot.\n",
					__func__
				);
				state = RECOVERY_KEY_STATE_TOO_LONG;
			}
		}

		tick_elapsed = get_ticks() - tick_base;
	}

	printf("%s: Key status. kfr=%d, press_ms=%llu\n",
		__func__, kfr, (unsigned long long)(press_ms)
	);

	if (press_ms < KEY_ENOUGH_TIME_MS)
		return 0;

	if (press_ms >= KEY_PRESS_MAX_TIME_MS)
		return 0;

	return 1;
}
#endif /*CONFIG_ANDROID_RECOVERY*/
#if (defined(CONFIG_ICX_FASTBOOT_MAGIC_KEY))
int is_fastboot_key_pressing(void)
{
	int	kff = -1;
	int	state = RECOVERY_KEY_STATE_START;

	uint64_t		tick_base = 0;
	uint64_t		tick_elapsed = 0;
	unsigned long long	press_ms = 0;

	tick_base = get_ticks();

	while ((press_ms = tick_to_ms(tick_elapsed))
		<= KEY_PRESS_MAX_TIME_MS) {

		kff = gpio_get_value(ICX_DMP_KEY_FF);
		if (kff != ICX_DMP_KEY_PRESSED)
			break;

		switch (state) {
		case RECOVERY_KEY_STATE_START:
			if (press_ms >= KEY_ENOUGH_TIME_MS) {
				printf("%s: Accept fastboot (uuu), Release Key.\n",
					__func__
				);
				state = RECOVERY_KEY_STATE_ACCEPT;
			}
			break;
		case RECOVERY_KEY_STATE_ACCEPT:
			if (press_ms >= KEY_PRESS_MAX_TIME_MS) {
				printf("%s: Pressing key too long, Normal boot.\n",
					__func__
				);
				state = RECOVERY_KEY_STATE_TOO_LONG;
			}
		}

		tick_elapsed = get_ticks() - tick_base;
	}

	printf("%s: Key status. kff=%d, press_ms=%llu\n",
		__func__, kff, (unsigned long long)press_ms
	);

	if (press_ms < KEY_ENOUGH_TIME_MS)
		return 0;

	if (press_ms >= KEY_PRESS_MAX_TIME_MS)
		return 0;

	set_bq25898_current(sony_bq25898, 500);
	/* for device locked */
	env_set("force_unlock","true");
	return 1;
}

int icx_fastboot_force_unlock(void)
{
	set_bq25898_current(sony_bq25898, 500);
	/* for device locked */
	env_set("force_unlock","true");

	return 0;
}
#endif /* (defined(CONFIG_ICX_FASTBOOT_MAGIC_KEY)) */
#endif /*CONFIG_FSL_FASTBOOT*/
