#include <linux/kernel.h>
#include <linux/of_platform.h>

#include <asm/machdep.h>
#include <asm/udbg.h>

#include "drivers/espresso-pic.h"
#include "drivers/latte-ahball-pic.h"

#include "power.h"

static int __init wiiu_probe(void) {
    if (!of_machine_is_compatible("nintendo,wiiu")) {
        return 0;
    }
    pm_power_off = &wiiu_power_off;
    return 1;
}

static void wiiu_init_irq(void) {
    espresso_pic_probe();
    latte_ahball_pic_probe();
}

define_machine(wiiu) {
    .name            = "wiiu",
    .probe           = wiiu_probe,
    .restart         = wiiu_restart,
    .halt            = wiiu_halt,
    .progress        = udbg_progress,
    .calibrate_decr  = generic_calibrate_decr,
    .init_IRQ        = wiiu_init_irq,
    .get_irq         = espresso_pic_get_irq,
    .restart         = wiiu_restart,
};

static const struct of_device_id wiiu_of_bus[] = {
    { .compatible = "nintendo,latte", },
    { },
};

static int __init wiiu_device_probe(void)
{
    if (!machine_is(wiiu))
        return 0;

    of_platform_populate(NULL, wiiu_of_bus, NULL, NULL);
    return 0;
}
device_initcall(wiiu_device_probe);
