/*
 * drivers/video/fbdev/wiiufb.c
 *
 * Framebuffer driver for Nintendo Wii U
 * Copyright (C) 2018 Ash Logan <quarktheawesome@gmail.com>
 * Copyright (C) 2018 Roberto Van Eeden <rwrr0644@gmail.com>
 *
 * Based on xilinxfb.c
 * Copyright (C) 2002-2007 MontaVista Software, Inc.
 * Copyright (C) 2007 Secret Lab Technologies, Ltd.
 * Copyright (C) 2009 Xilinx Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 */

#include <linux/types.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/vt.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/compat.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/console.h>
#include <linux/kmod.h>
#include <linux/err.h>
#include <linux/efi.h>
#include <linux/delay.h>
#include <stdarg.h>
#include <asm/cacheflush.h>
#include <asm/io.h>
#include <asm/fb.h>

#include "wiiufb_regs.h"

#define DRIVER_NAME		"wiiufb"

//Useful for reading stuff out from the GX2's MMIO
static int leak_mmio = 0xDEADCAFE;
module_param(leak_mmio, int, 0644);
MODULE_PARM_DESC(leak_mmio, "oops");

/*
 * In default fb mode we only support a single mode: 1280x720 32 bit true color.
 * Each pixel gets a word (32 bits) of memory, organized as RGBA8888
 */

// 16 bit
#define BYTES_PER_PIXEL	2
#define BITS_PER_PIXEL	(BYTES_PER_PIXEL * 8)

// RGB565
#define OFFS_RED		11
#define OFFS_GREEN		5
#define OFFS_BLUE		0

// Use 16 palettes
#define MAX_PALETTES	16

struct wiiufb_platform_data {
	u32 width, height;         /* resolution of screen in pixels */
};

static struct fb_fix_screeninfo wiiu_fb_fix = {
	.id =		"wiiufb",
	.type =		FB_TYPE_PACKED_PIXELS,
	.visual =	FB_VISUAL_TRUECOLOR,
	.accel =	FB_ACCEL_NONE
};

static struct fb_var_screeninfo wiiu_fb_var = {
	.bits_per_pixel =	BITS_PER_PIXEL,

	.red =		{ OFFS_RED, 5, 0 },
	.green =	{ OFFS_GREEN, 6, 0 },
	.blue =		{ OFFS_BLUE, 5, 0 },
	.transp =	{ 0, 0, 0 },

	.activate =	FB_ACTIVATE_NOW
};

struct wiiufb_drvdata {
	struct fb_info info;								/* FB driver info record */
	void __iomem *regs;									/* virt. address of the control registers */
	phys_addr_t	  regs_phys;
	void         *fb_virt;								/* virt. address of the frame buffer */
	dma_addr_t    fb_phys;								/* phys. address of the frame buffer */
	u32 	      pseudo_palette[MAX_PALETTES];			/* Fake palette of 16 colors */
};

static int wiiufb_setcolreg(unsigned regno, unsigned red, unsigned green, unsigned blue, unsigned transp, struct fb_info *info) {
	u32 *pal = info->pseudo_palette;

	if (regno >= MAX_PALETTES)
		return -EINVAL;

	// convert RGB to grayscale
	if (info->var.grayscale)
		red = green = blue = (19595 * red + 38470 * green + 7471 * blue) >> 16;

	// 16 bit RGB565
	pal[regno] = (red & 0xf800) | ((green & 0xfc00) >> 5) | ((blue & 0xf800) >> 11);
	return 0;
}

/*
 * Since there's no cache coherency between Espresso and Latte, the framebuffer
 * must be mapped with write-trough caching or with caching disabled
 */
static int wiiufb_mmap(struct fb_info *info, struct vm_area_struct * vma)
{
	unsigned long mmio_pgoff;
	unsigned long start;
	u32 len;

	start = info->fix.smem_start;
	len = info->fix.smem_len;
	mmio_pgoff = PAGE_ALIGN((start & ~PAGE_MASK) + len) >> PAGE_SHIFT;
	if (vma->vm_pgoff >= mmio_pgoff) {
		if (info->var.accel_flags)
			return -EINVAL;

		vma->vm_pgoff -= mmio_pgoff;
		start = info->fix.mmio_start;
		len = info->fix.mmio_len;
	}
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	return vm_iomap_memory(vma, start, len);
}


static struct fb_ops wiiufb_ops = {
	.owner				= THIS_MODULE,
	.fb_setcolreg		= wiiufb_setcolreg,
	.fb_mmap			= wiiufb_mmap,
	.fb_fillrect		= cfb_fillrect,
	.fb_copyarea		= cfb_copyarea,
	.fb_imageblit		= cfb_imageblit,
};


static int wiiufb_assign(struct platform_device *pdev, struct wiiufb_drvdata *drvdata, struct wiiufb_platform_data *pdata) {
	int rc;
	struct device *dev = &pdev->dev;
	int fbsize = pdata->width * pdata->height * BYTES_PER_PIXEL;
	struct resource* res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	drvdata->regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(drvdata->regs)) {
		dev_err(dev, "Failed to map registers!\n");
		return -ENOMEM;
	}

	drvdata->regs_phys = res->start;
	drvdata->fb_virt = dma_zalloc_coherent(dev, PAGE_ALIGN(fbsize), &drvdata->fb_phys, GFP_KERNEL);

	if (!drvdata->fb_virt) {
		dev_err(dev, "Could not allocate framebuffer!\n");
		return -ENOMEM;
	}

	writereg(DGRPH_ENABLE, 0);
	writereg(DGRPH_CONTROL, 0);
	writereg(DGRPH_PRIMARY_SURFACE_ADDRESS, 0);
	writereg(DGRPH_PITCH, 0);
	setreg(DGRPH_ENABLE, DGRPH_ENABLE_REG, 1);
	setreg(DGRPH_CONTROL, DGRPH_DEPTH, DGRPH_DEPTH_16BPP);
	setreg(DGRPH_CONTROL, DGRPH_FORMAT, DGRPH_FORMAT_16BPP_RGB565);
	setreg(DGRPH_CONTROL, DGRPH_ADDRESS_TRANSLATION, DGRPH_ADDRESS_TRANSLATION_PHYS);
	setreg(DGRPH_CONTROL, DGRPH_PRIVILEGED_ACCESS, DGRPH_PRIVILEGED_ACCESS_DISABLE);
	setreg(DGRPH_CONTROL, DGRPH_ARRAY_MODE, DGRPH_ARRAY_LINEAR_ALIGNED);
	setreg(DGRPH_PRIMARY_SURFACE_ADDRESS, DGRPH_PRIMARY_SURFACE_ADDR, drvdata->fb_phys);
	setreg(DGRPH_PITCH, DGRPH_PITCH_VAL, pdata->width);
	leak_mmio = readreg(DGRPH_CONTROL);

	setreg(DGRPH_SWAP_CNTL, DGRPH_ENDIAN_SWAP, DGRPH_ENDIAN_SWAP_16);
	setreg(DGRPH_SWAP_CNTL, DGRPH_RED_CROSSBAR, DGRPH_RED_CROSSBAR_RED);
	setreg(DGRPH_SWAP_CNTL, DGRPH_GREEN_CROSSBAR, DGRPH_GREEN_CROSSBAR_GREEN);
	setreg(DGRPH_SWAP_CNTL, DGRPH_BLUE_CROSSBAR, DGRPH_BLUE_CROSSBAR_BLUE);
	setreg(DGRPH_SWAP_CNTL, DGRPH_ALPHA_CROSSBAR, DGRPH_ALPHA_CROSSBAR_ALPHA);

	/* Fill struct fb_info */
	drvdata->info.device = dev;
	drvdata->info.screen_base = drvdata->fb_virt;
	drvdata->info.screen_size = fbsize;
	drvdata->info.fbops = &wiiufb_ops;
	drvdata->info.fix = wiiu_fb_fix;
	drvdata->info.fix.smem_start = drvdata->fb_phys;
	drvdata->info.fix.smem_len = fbsize;
	drvdata->info.fix.line_length = pdata->width * BYTES_PER_PIXEL;
	drvdata->info.fix.mmio_start = res->start;
	drvdata->info.fix.mmio_len = resource_size(res);

	drvdata->info.pseudo_palette = drvdata->pseudo_palette;
	drvdata->info.flags = FBINFO_DEFAULT;
	drvdata->info.var = wiiu_fb_var;
	drvdata->info.var.xres = pdata->width;
	drvdata->info.var.yres = pdata->height;
	drvdata->info.var.xres_virtual = pdata->width;
	drvdata->info.var.yres_virtual = pdata->height;

	/* Allocate a colour map */
	rc = fb_alloc_cmap(&drvdata->info.cmap, MAX_PALETTES, 0);
	if (rc) {
		return rc;
	}

	/* Register new frame buffer */
	rc = register_framebuffer(&drvdata->info);
	if (rc) {
		fb_dealloc_cmap(&drvdata->info.cmap);
		return rc;
	}

	return 0;
}

static int wiiufb_release(struct device *dev) {
	struct wiiufb_drvdata *drvdata = dev_get_drvdata(dev);

	unregister_framebuffer(&drvdata->info);
	fb_dealloc_cmap(&drvdata->info.cmap);

	return 0;
}

static int wiiufb_probe(struct platform_device *pdev) {
	struct wiiufb_platform_data pdata;
	struct wiiufb_drvdata *drvdata;

	if (of_property_read_u32(pdev->dev.of_node, "default-width", &pdata.width)) {
		pdata.width = 1280;
	}
	if (of_property_read_u32(pdev->dev.of_node, "default-height", &pdata.height)) {
		pdata.height = 720;
	}

	pr_info("wiiufb: making %dx%d framebuffer\n", pdata.width, pdata.height);

	/* Allocate the driver data region */
	drvdata = devm_kzalloc(&pdev->dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata) {
		return -ENOMEM;
	}

	dev_set_drvdata(&pdev->dev, drvdata);
	return wiiufb_assign(pdev, drvdata, &pdata);
}

static int wiiufb_remove(struct platform_device *op) {
	return wiiufb_release(&op->dev);
}

/* Match table for of_platform binding */
static struct of_device_id wiiufb_match[] = {
	{ .compatible = "nintendo,wiiufb", },
	{},
};
MODULE_DEVICE_TABLE(of, wiiufb_match);

static struct platform_driver wiiufb_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = wiiufb_match,
	},
	.probe  = wiiufb_probe,
	.remove = wiiufb_remove,
};
module_platform_driver(wiiufb_driver);

MODULE_DESCRIPTION("Wii U framebuffer driver");
MODULE_LICENSE("GPL");
