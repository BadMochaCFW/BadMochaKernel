/*
 * drivers/video/fbdev/wiiufb_regs.h
 *
 * Framebuffer registers for Nintendo Wii U GPU
 * Copyright (C) 2018 Ash Logan <quarktheawesome@gmail.com>
 * Copyright (C) 2018 Roberto Van Eeden <rwrr0644@gmail.com>
 *
 * Based on AMD RV630 Reference Guide
 * Copyright (C) 2007 Advanced Micro Devices, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 */

#define readreg(addr) ioread32be(drvdata->regs + addr)
#define writereg(addr, val) iowrite32be(val, drvdata->regs + addr)

#define setreg(addr, reg, val) writereg(addr, (readreg(addr) & ~reg) | (val & reg))

#define DGRPH_ENABLE 0x0100
    #define DGRPH_ENABLE_REG 0x1
#define DGRPH_CONTROL 0x0104
    #define DGRPH_DEPTH 0x3
        #define DGRPH_DEPTH_8BPP 0x0
        #define DGRPH_DEPTH_16BPP 0x1
        #define DGRPH_DEPTH_32BPP 0x2
        #define DGRPH_DEPTH_64BPP 0x3
    #define DGRPH_FORMAT 0x700
        #define DGRPH_FORMAT_8BPP_INDEXED 0x000
        #define DGRPH_FORMAT_16BPP_ARGB1555 0x000
        #define DGRPH_FORMAT_16BPP_RGB565 0x100
        #define DGRPH_FORMAT_16BPP_ARGB4444 0x200
        //todo: 16bpp alpha index 88, mono 16, brga 5551
        #define DGRPH_FORMAT_32BPP_ARGB8888 0x000
        #define DGRPH_FORMAT_32BPP_ARGB2101010 0x100
        #define DGRPH_FORMAT_32BPP_DIGITAL 0x200
        #define DGRPH_FORMAT_32BPP_8ARGB2101010 0x300
        #define DGRPH_FORMAT_32BPP_BGRA1010102 0x400
        #define DGRPH_FORMAT_32BPP_8BGRA1010102 0x500
        #define DGRPH_FORMAT_32BPP_RGB111110 0x600
        #define DGRPH_FORMAT_32BPP_BGR101111 0x700
        //todo: 64bpp
    #define DGRPH_ADDRESS_TRANSLATION 0x10000
        #define DGRPH_ADDRESS_TRANSLATION_PHYS 0x00000
        #define DGRPH_ADDRESS_TRANSLATION_VIRT 0x10000
    #define DGRPH_PRIVILEGED_ACCESS 0x20000
        #define DGRPH_PRIVILEGED_ACCESS_DISABLE 0x00000
        #define DGRPH_PRIVILEGED_ACCESS_ENABLE 0x20000
    #define DGRPH_ARRAY_MODE 0xF00000
        #define DGRPH_ARRAY_LINEAR_GENERAL 0x000000
        #define DGRPH_ARRAY_LINEAR_ALIGNED 0x100000
        #define DGRPH_ARRAY_1D_TILES_THIN1 0x200000
        //todo: rest of these array modes
		
#define DGRPH_SWAP_CNTL 0x010C
	#define DGRPH_ENDIAN_SWAP 0x3
		#define DGRPH_ENDIAN_SWAP_NONE 0x0
		#define DGRPH_ENDIAN_SWAP_16 0x1
		#define DGRPH_ENDIAN_SWAP_32 0x2
		#define DGRPH_ENDIAN_SWAP_64 0x3
	#define DGRPH_RED_CROSSBAR 0x30
		#define DGRPH_RED_CROSSBAR_RED 0x00
	#define DGRPH_GREEN_CROSSBAR 0xC0
		#define DGRPH_GREEN_CROSSBAR_GREEN 0x00
	#define DGRPH_BLUE_CROSSBAR 0x300
		#define DGRPH_BLUE_CROSSBAR_BLUE 0x000
	#define DGRPH_ALPHA_CROSSBAR 0xC00
		#define DGRPH_ALPHA_CROSSBAR_ALPHA 0x000
	// todo: other values for crossbars

#define DGRPH_PRIMARY_SURFACE_ADDRESS 0x0110
    #define DGRPH_PRIMARY_DFQ_ENABLE 0x1
        #define DGRPH_PRIMARY_DFQ_OFF 0x0
        #define DGRPH_PRIMARY_DFQ_ON 0x1
    #define DGRPH_PRIMARY_SURFACE_ADDR 0xFFFFFF00

#define DGRPH_PITCH 0x0120
    #define DGRPH_PITCH_VAL 0x3FFF
