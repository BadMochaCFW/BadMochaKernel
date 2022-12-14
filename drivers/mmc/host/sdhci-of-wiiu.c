/*
 * drivers/mmc/host/sdhci-of-wiiu.c
 *
 * Nintendo Wii U Secure Digital Host Controller Interface.
 *
 * Based on sdhci-of-hlwd.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mmc/host.h>
#include "sdhci-pltfm.h"

/*
 * Ops and quirks for the Nintendo Wii U SDHCI controllers.
 */

/*
 * We need a small delay after each write, or things go horribly wrong.
 */
#define SDHCI_wiiu_WRITE_DELAY	10 /* usecs */

static u32 sdhci_wiiu_readl(struct sdhci_host *host, int reg) {
	return in_be32(host->ioaddr + reg);
}

static u16 sdhci_wiiu_readw(struct sdhci_host *host, int reg) {
	if(reg & 3)
		return (in_be32((u32*)(((u32)host->ioaddr + reg) & ~3)) & 0xffff0000) >> 16;
	else
		return (in_be32(host->ioaddr + reg) & 0xffff);
}

static u8 sdhci_wiiu_readb(struct sdhci_host *host, int reg) {
	u8 shift = (reg & 3) * 8;
	u32 mask = (0xFF << shift);
	u32 addr = (u32)host->ioaddr + reg;

	return (in_be32((u32*)(addr & ~3)) & mask) >> shift;
}

static void sdhci_wiiu_writel(struct sdhci_host *host, u32 val, int reg) {
	out_be32(host->ioaddr + reg, val);
	udelay(SDHCI_wiiu_WRITE_DELAY);
}

static void sdhci_wiiu_writew(struct sdhci_host *host, u16 val, int reg) {
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	
	switch (reg) {
		case SDHCI_TRANSFER_MODE:
			// Postpone this write, we must do it together with a command write that is down below
			pltfm_host->xfer_mode_shadow = val;
			return;
		case SDHCI_COMMAND:
			sdhci_wiiu_writel(host, val << 16 | pltfm_host->xfer_mode_shadow, SDHCI_TRANSFER_MODE);
			return;
	}
	
	if(reg & 3)
		clrsetbits_be32((u32*)(((u32)host->ioaddr + reg) & ~3), 0xffff0000, val << 16);
	else
		clrsetbits_be32(host->ioaddr + reg, 0xffff, ((u32)val));
	udelay(SDHCI_wiiu_WRITE_DELAY);
}

static void sdhci_wiiu_writeb(struct sdhci_host *host, u8 val, int reg) {
	u8 shift = (reg & 3) * 8;
	u32 mask = (0xFF << shift);
	u32 addr = (u32)host->ioaddr + reg;

	clrsetbits_be32((u32*)(addr & ~3), mask, val << shift);
	udelay(SDHCI_wiiu_WRITE_DELAY);
}

static const struct sdhci_ops sdhci_wiiu_ops = {
	.read_l = sdhci_wiiu_readl,
	.read_w = sdhci_wiiu_readw,
	.read_b = sdhci_wiiu_readb,
	.write_l = sdhci_wiiu_writel,
	.write_w = sdhci_wiiu_writew,
	.write_b = sdhci_wiiu_writeb,
	.set_clock = sdhci_set_clock,
	.set_bus_width = sdhci_set_bus_width,
	.reset = sdhci_reset,
	.set_uhs_signaling = sdhci_set_uhs_signaling,
};

static const struct sdhci_pltfm_data sdhci_wiiu_pdata = {
	.quirks = SDHCI_QUIRK_32BIT_DMA_ADDR | SDHCI_QUIRK_32BIT_DMA_SIZE,
	.ops = &sdhci_wiiu_ops,
};

static int sdhci_wiiu_probe(struct platform_device *pdev)
{
	return sdhci_pltfm_register(pdev, &sdhci_wiiu_pdata, 0);
}

static const struct of_device_id sdhci_wiiu_of_match[] = {
	{ .compatible = "nintendo,wiiu-sdhci" },
	{ }
};
MODULE_DEVICE_TABLE(of, sdhci_wiiu_of_match);

static struct platform_driver sdhci_wiiu_driver = {
	.driver = {
		.name = "sdhci-wiiu",
		.of_match_table = sdhci_wiiu_of_match,
		.pm = &sdhci_pltfm_pmops,
	},
	.probe = sdhci_wiiu_probe,
	.remove = sdhci_pltfm_unregister,
};

module_platform_driver(sdhci_wiiu_driver);
