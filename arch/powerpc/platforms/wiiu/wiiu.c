#include <linux/of_platform.h>

#include <asm/machdep.h>
#include <asm/udbg.h>

static int __init wiiu_probe(void) {
    if (!of_machine_is_compatible("nintendo,wiiu")) {
        return 0;
    }
    
    return 1;
}

define_machine(wiiu) {
    .name           = "wiiu",
    .probe          = wiiu_probe,
    .progress       = udbg_progress,
    .calibrate_decr = generic_calibrate_decr,
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
