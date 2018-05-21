/*
 * arch/powerpc/boot/wiiu.c
 *
 * Nintendo Wii U
 * Copyright (C) 2018 Ash Logan <quarktheawesome@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 */

#include <stddef.h>
#include "string.h"
#include "stdio.h"
#include "types.h"
#include "io.h"
#include "ops.h"

BSS_STACK(8192);

#define LT_IPC_PPCMSG  (volatile u32 volatile*)0x0d800000
#define LT_IPC_PPCCTRL (volatile u32 volatile*)0x0d800004

#define WIIU_LOADER_CMD_PRINT 0x01000000
/* If this struct ever has to be changed in a non-ABI compatible way,
   change the magic.
   Past magics:
    - 0xCAFEFECA: initial version
*/
#define WIIU_LOADER_MAGIC 0xCAFEFECA
struct wiiu_loader_data {
    unsigned int magic;
    char cmdline[256];
    void* initrd;
    unsigned int initrd_sz;
};
const static struct wiiu_loader_data* arm_data = (void*)0x89200000;

static void wiiu_copy_cmdline(char* cmdline, int cmdlineSz, unsigned int timeout) {
/*  If the ARM left us a commandline, copy it in */
    /*if (arm_data->magic == WIIU_LOADER_MAGIC) {
        strncpy(cmdline, arm_data->cmdline, 256);
    }*/
}

static void wiiu_write_ipc(const char *buf, int len) {
    int i = 0;
    for (i = 0; i < len; i += 3) {
        *LT_IPC_PPCMSG = WIIU_LOADER_CMD_PRINT
            | (buf[i + 0] << 16) | (buf[i + 1] << 8) | buf[i + 2];

        *LT_IPC_PPCCTRL = 1;

        while (*LT_IPC_PPCCTRL & 1) {}
    }
    if (i < len) {
        for (; i < len; i++) {
            *LT_IPC_PPCMSG = WIIU_LOADER_CMD_PRINT
                | (buf[i] << 16);

            *LT_IPC_PPCCTRL = 1;

            while (*LT_IPC_PPCCTRL & 1);
        }
    }
}

/* Mostly copied from gamecube.c. Obviously the GameCube is not the same
 * as the Wii U. TODO.
 */
void platform_init(unsigned int r3, unsigned int r4, unsigned int r5) {
    u32 heapsize;

    console_ops.write = wiiu_write_ipc;
    wiiu_write_ipc("Hello from the bootwrapper!\n", sizeof("Hello from the bootwrapper!\n"));
    

    heapsize = 16*1024*1024 - (u32)_end;
    simple_alloc_init(_end, heapsize, 32, 64);

    wiiu_write_ipc("heap ok\n", sizeof("heap ok\n"));
    
    fdt_init(_dtb_start);

    wiiu_write_ipc("dtb ok\n", sizeof("dtb ok\n"));

/*  TODO re-add the mapping for 0x89200000 to enable this */
    /*console_ops.edit_cmdline = wiiu_copy_cmdline;
    if (arm_data->magic == WIIU_LOADER_MAGIC) {
        if (arm_data->initrd_sz > 0) {
            loader_info.initrd_addr = (unsigned long)arm_data->initrd;
            loader_info.initrd_size = (unsigned long)arm_data->initrd_sz;
        }
    }*/
}
