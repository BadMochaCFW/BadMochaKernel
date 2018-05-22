#include <mm/mmu_decl.h>

#include <asm/io.h>
#include <asm/prom.h>
#include <asm/udbg.h>
#include <asm/fixmap.h>

#define LT_MMIO_BASE    (phys_addr_t)0x0d800000
#define LT_IPC_PPCMSG   0x00
#define LT_IPC_PPCCTRL  0x04
#define LT_IPC_PPCCTRL_X1 0x01

#define WIIU_LOADER_CMD_PRINT 0x01000000

void __iomem *latte_mmio;

void udbg_putc_latteipc(char c) {
    out_be32(latte_mmio + LT_IPC_PPCMSG, WIIU_LOADER_CMD_PRINT | (c << 16));
    out_be32(latte_mmio + LT_IPC_PPCCTRL, LT_IPC_PPCCTRL_X1);

    while (in_be32(latte_mmio + LT_IPC_PPCCTRL) & LT_IPC_PPCCTRL_X1) {
        barrier();
    }
}

void __init udbg_init_latteipc(void) {
    latte_mmio = (void __iomem*)__fix_to_virt(FIX_EARLY_DEBUG_BASE);

    udbg_putc = udbg_putc_latteipc;

    setbat(1, (unsigned long)latte_mmio, LT_MMIO_BASE, 128*1024, PAGE_KERNEL_NCG);
}