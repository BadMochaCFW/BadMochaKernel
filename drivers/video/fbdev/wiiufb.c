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
#include <linux/of_reserved_mem.h>
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

// Use 16 palettes
#define MAX_PALETTES	16

struct wiiufb_drvdata {
	void __iomem *regs;									/* virt. address of the control registers */
	dma_addr_t    screen_dma;							/* dma handle of the frame buffer */
	u32 	      pseudo_palette[MAX_PALETTES];			/* Fake palette of 16 colors */
};

static int wiiufb_check_var(struct fb_var_screeninfo *var, struct fb_info *info) {
	uint32_t line_length;

	if (!var->xres)
		var->xres = 1;
	if (!var->yres)
		var->yres = 1;
	if (var->xres > var->xres_virtual)
		var->xres_virtual = var->xres;
	if (var->yres > var->yres_virtual)
		var->yres_virtual = var->yres;
	if (var->bits_per_pixel <= 16)
		var->bits_per_pixel = 16;
	else if (var->bits_per_pixel <= 32)
		var->bits_per_pixel = 32;
	else
		return -EINVAL;

	if (var->xres_virtual < var->xoffset + var->xres)
		var->xres_virtual = var->xoffset + var->xres;
	if (var->yres_virtual < var->yoffset + var->yres)
		var->yres_virtual = var->yoffset + var->yres;

	// Round up to nearest 32/0x20
	// https://stackoverflow.com/a/9194117
	var->xres_virtual = (var->xres_virtual + 0x20 - 1) & -0x20;

	line_length =
		var->xres_virtual *
		(var->bits_per_pixel / 8);
	//if (line_length * var->yres_virtual > ) return -ENOMEM;

	if (var->bits_per_pixel == 16) {
		//RGB565
		var->red   = (struct fb_bitfield) { .offset = 11,  .length = 5, };
		var->green = (struct fb_bitfield) { .offset = 5,  .length = 6, };
		var->blue  = (struct fb_bitfield) { .offset = 0, .length = 5, };
		var->transp= (struct fb_bitfield) { .offset = 0,  .length = 0, };
	} else if (var->bits_per_pixel == 32) {
		//ARGB8888
		var->red   = (struct fb_bitfield) { .offset = 8,  .length = 8, };
		var->green = (struct fb_bitfield) { .offset = 16, .length = 8, };
		var->blue  = (struct fb_bitfield) { .offset = 24, .length = 8, };
		var->transp= (struct fb_bitfield) { .offset = 0,  .length = 8, };
	}
	var->grayscale = 0;

	return 0;
}

static int wiiufb_set_par(struct fb_info *info) {
	struct wiiufb_drvdata *drvdata = (struct wiiufb_drvdata*)info->par;

	writereg(DGRPH_ENABLE, 0);
	writereg(DGRPH_CONTROL, 0);
	writereg(DGRPH_PRIMARY_SURFACE_ADDRESS, 0);
	writereg(DGRPH_PITCH, 0);

	if (info->var.bits_per_pixel == 16) {
		setreg(DGRPH_CONTROL, DGRPH_DEPTH, DGRPH_DEPTH_16BPP);
		setreg(DGRPH_CONTROL, DGRPH_FORMAT, DGRPH_FORMAT_16BPP_RGB565);
		setreg(DGRPH_SWAP_CNTL, DGRPH_ENDIAN_SWAP, DGRPH_ENDIAN_SWAP_16);
	} else if (info->var.bits_per_pixel == 32) {
		setreg(DGRPH_CONTROL, DGRPH_DEPTH, DGRPH_DEPTH_32BPP);
		setreg(DGRPH_SWAP_CNTL, DGRPH_ENDIAN_SWAP, DGRPH_ENDIAN_SWAP_32);
	}

	setreg(DGRPH_CONTROL, DGRPH_ADDRESS_TRANSLATION, DGRPH_ADDRESS_TRANSLATION_PHYS);
	setreg(DGRPH_CONTROL, DGRPH_PRIVILEGED_ACCESS, DGRPH_PRIVILEGED_ACCESS_DISABLE);
	setreg(DGRPH_CONTROL, DGRPH_ARRAY_MODE, DGRPH_ARRAY_LINEAR_ALIGNED);
	setreg(DGRPH_SWAP_CNTL, DGRPH_RED_CROSSBAR, DGRPH_RED_CROSSBAR_RED);
	setreg(DGRPH_SWAP_CNTL, DGRPH_GREEN_CROSSBAR, DGRPH_GREEN_CROSSBAR_GREEN);
	setreg(DGRPH_SWAP_CNTL, DGRPH_BLUE_CROSSBAR, DGRPH_BLUE_CROSSBAR_BLUE);
	setreg(DGRPH_SWAP_CNTL, DGRPH_ALPHA_CROSSBAR, DGRPH_ALPHA_CROSSBAR_ALPHA);

	setreg(DGRPH_PITCH, DGRPH_PITCH_VAL, info->var.xres_virtual);
	info->fix.line_length = info->var.xres_virtual * info->var.bits_per_pixel / 8;

	setreg(DGRPH_SURFACE_OFFSET_X, DGRPH_SURFACE_OFFSET_X_VAL, info->var.xoffset);
	setreg(DGRPH_SURFACE_OFFSET_Y, DGRPH_SURFACE_OFFSET_Y_VAL, info->var.yoffset);
	setreg(DGRPH_X_START, DGRPH_X_START_VAL, 0);
	setreg(DGRPH_Y_START, DGRPH_Y_START_VAL, 0);
	setreg(DGRPH_X_END, DGRPH_X_END_VAL, info->var.xres);
	setreg(DGRPH_Y_END, DGRPH_Y_END_VAL, info->var.yres);

	if (info->screen_base) {
		dma_free_coherent(info->device, info->screen_size, info->screen_base, drvdata->screen_dma);
		info->screen_base = NULL;
		info->screen_size = 0;
		drvdata->screen_dma = 0;
	}
	info->screen_size = PAGE_ALIGN(info->fix.line_length * info->var.yres_virtual);
	info->screen_base = dma_zalloc_coherent(info->device, info->screen_size, &drvdata->screen_dma, GFP_KERNEL);
	if (!info->screen_base) {
		fb_err(info, "Could not allocate frambuffer memory!\n");
		return -ENOMEM;
	}
	info->fix.smem_start = drvdata->screen_dma;
	info->fix.smem_len = info->screen_size;
	fb_info(info, "Allocated framebuffer %p / 0x%08x\n", info->screen_base, drvdata->screen_dma);

	setreg(DGRPH_PRIMARY_SURFACE_ADDRESS, DGRPH_PRIMARY_SURFACE_ADDR, drvdata->screen_dma);
	setreg(DGRPH_ENABLE, DGRPH_ENABLE_REG, 1);

	return 0;
}

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
	.fb_check_var		= wiiufb_check_var,
	.fb_set_par			= wiiufb_set_par,
//	.fb_mmap			= wiiufb_mmap,
	.fb_fillrect		= cfb_fillrect,
	.fb_copyarea		= cfb_copyarea,
	.fb_imageblit		= cfb_imageblit,
};

static struct fb_fix_screeninfo wiiufb_fix = {
	.id =		"wiiufb",
	.type =		FB_TYPE_PACKED_PIXELS,
	.visual =	FB_VISUAL_TRUECOLOR,
	.accel =	FB_ACCEL_NONE
};

static struct fb_var_screeninfo wiiufb_var_default = {
	.bits_per_pixel =	16,
	.activate =	FB_ACTIVATE_NOW,
};

static int wiiufb_probe(struct platform_device *pdev) {
	struct wiiufb_drvdata *drvdata;
	struct resource *regs;
	struct fb_info *info;
	int rc;
	u32 width, height;

	info = framebuffer_alloc(sizeof(struct wiiufb_drvdata), &pdev->dev);
	if (!info) {
		pr_err("wiiufb: Couldn't allocate fb_info!\n");
		return -ENOMEM;
	}
	drvdata = (struct wiiufb_drvdata *)info->par;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	drvdata->regs = devm_ioremap_resource(&pdev->dev, regs);
	if (IS_ERR(drvdata->regs)) {
		dev_err(&pdev->dev, "Failed to map registers!\n");
		return -ENOMEM;
	}

	rc = of_reserved_mem_device_init(&pdev->dev);
	if (rc) {
		dev_err(&pdev->dev, "Failed to get reserved memory! %d\n", rc);
		return -ENOMEM;
	}

	if (of_property_read_u32(pdev->dev.of_node, "default-width", &width)) {
		width = 1280;
	}
	if (of_property_read_u32(pdev->dev.of_node, "default-height", &height)) {
		height = 720;
	}

	dev_info(&pdev->dev, "making %dx%d framebuffer\n", width, height);

	info->var = wiiufb_var_default;
	info->var.xres = width;
	info->var.yres = height;
	info->var.xres_virtual = width;
	info->var.yres_virtual = height;

	info->fix = wiiufb_fix;
	info->fix.mmio_start = regs->start;
	info->fix.mmio_len = resource_size(regs);

	info->flags = FBINFO_FLAG_DEFAULT;
	info->pseudo_palette = &drvdata->pseudo_palette;
	info->fbops = &wiiufb_ops;

	/* Allocate a colour map */
	rc = fb_alloc_cmap(&info->cmap, MAX_PALETTES, 0);
	if (rc) {
		return rc;
	}

	wiiufb_check_var(&info->var, info);

	/* Register new frame buffer */
	rc = register_framebuffer(info);
	if (rc) {
		fb_dealloc_cmap(&info->cmap);
		return rc;
	}
	platform_set_drvdata(pdev, info);

	return 0;
}

static int wiiufb_remove(struct platform_device *pdev) {
	struct fb_info *info = platform_get_drvdata(pdev);

	if (info) {
		unregister_framebuffer(info);
		fb_dealloc_cmap(&info->cmap);
		framebuffer_release(info);
	}

	return 0;
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
