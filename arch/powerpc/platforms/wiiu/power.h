#ifndef _WIIU_POWER_H
#define _WIIU_POWER_H

#include <linux/kernel.h>

void __noreturn wiiu_halt(void);
void __noreturn wiiu_power_off(void);
void __noreturn wiiu_restart(char *cmd);

#endif

