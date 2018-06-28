/*
 * arch/powerpc/platform/wiiu/power.c
 * Power control with latte ipc
 */

#include <linux/kernel.h>
#include <linux/of_address.h>
#include <linux/of.h>

#define LT_IPC_PPCMSG_COMPAT 0x0
#define LT_IPC_PPCCTRL_COMPAT 0x4
#define LT_CTRL_X1 0x1

enum latte_cmds {
    CMD_SHUTDOWN = 0xCAFE0001,
    CMD_REBOOT   = 0xCAFE0002,
};

void __iomem *latte_ipc = NULL;

void latte_ipc_cmd(unsigned cmd) {
    if(latte_ipc == NULL) {
        struct resource res;
        struct device_node *np = of_find_compatible_node(NULL, NULL, "nintendo,latte-ipc");
        if(!np || of_address_to_resource(np, 0, &res))
            return;
        latte_ipc = ioremap(res.start, resource_size(&res));
        if(IS_ERR(latte_ipc))
            return;
        of_node_put(np);
    }
    out_be32(latte_ipc + LT_IPC_PPCMSG_COMPAT, cmd);
    setbits32(latte_ipc + LT_IPC_PPCCTRL_COMPAT, LT_CTRL_X1);
}

void __noreturn wiiu_halt(void) {
    for(;;) cpu_relax();
}

void __noreturn wiiu_power_off(void) {
    latte_ipc_cmd(CMD_SHUTDOWN);
    wiiu_halt();
}

void __noreturn wiiu_restart(char *cmd) {
    latte_ipc_cmd(CMD_REBOOT);
    wiiu_halt();
}
