/*
 * drivers/usb/host/ohci-wiiu.c
 *
 * Nintendo Wii U USB Open Host Controller Interface.
 * TODO: this is basically the same driver as the wii for now, but the wii u obviously differs
 *
 * Copyright (C) 2018 Ash Logan <ash@heyquark.com>
 * Copyright (C) 2017 rw-r-r-0644
 * Copyright (C) 2009 The GameCube Linux Team
 * Copyright (C) 2009 Albert Herranz
 *
 * Based on ohci-ppc-of.c and ohci-platform.c
 *
 * This file is licenced under the GPL.
 */

/*
 * TODO: override ohci_start or ohci_setup to do the ehci vendor interrupt enable guff
 */

#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/usb/ohci_pdriver.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>

#define HW_NAME "Nintendo Wii U OHCI Controller"

#define DRV_MODULE_NAME "ohci-wiiu"
#define DRV_DESCRIPTION "OHCI glue for " HW_NAME

static const struct hc_driver wiiu_ohci_hc_driver = {
    .description = DRV_MODULE_NAME,
    .product_desc = HW_NAME,
    .hcd_priv_size = sizeof(struct ohci_hcd),

    /*
     * generic hardware linkage
    */
    .irq = ohci_irq,
    .flags = HCD_MEMORY | HCD_USB11,

    /*
    * basic lifecycle operations
    */
    .reset = ohci_setup,
    .start = ohci_start,
    .stop = ohci_stop,
    .shutdown = ohci_shutdown,

    /*
     * managing i/o requests and associated device resources
    */
    .urb_enqueue = ohci_urb_enqueue,
    .urb_dequeue = ohci_urb_dequeue,
    .endpoint_disable = ohci_endpoint_disable,

    /*
    * scheduling support
    */
    .get_frame_number = ohci_get_frame,

    /*
    * root hub support
    */
    .hub_status_data = ohci_hub_status_data,
    .hub_control = ohci_hub_control,
#ifdef CONFIG_PM
    .bus_suspend = ohci_bus_suspend,
    .bus_resume = ohci_bus_resume,
#endif
    .start_port_reset = ohci_start_port_reset,
};

static int wiiu_ohci_probe(struct platform_device *op) {
    struct usb_hcd *hcd;
    struct ohci_hcd *ohci;
    struct resource *res_mem;
    int err, irq;

    if (usb_disabled()) return -ENODEV;

    //This is probably a bad thing.
    err = dma_coerce_mask_and_coherent(&op->dev, DMA_BIT_MASK(32));
    if (err) return err;

    irq = platform_get_irq(op, 0);
    if (irq < 0) {
        dev_err(&op->dev, "no irq provided!");
    }

    hcd = usb_create_hcd(&wiiu_ohci_hc_driver, &op->dev, dev_name(&op->dev));
    if (!hcd) return -ENOMEM;

    ohci = hcd_to_ohci(hcd);

    if (op->dev.of_node) {
        if (of_property_read_bool(op->dev.of_node, "big-endian-regs")) {
            ohci->flags |= OHCI_QUIRK_BE_MMIO;
        }
        if (of_property_read_bool(op->dev.of_node, "big-endian-desc")) {
            ohci->flags |= OHCI_QUIRK_BE_DESC;
        }
        if (of_property_read_bool(op->dev.of_node, "big-endian")) {
            ohci->flags |= OHCI_QUIRK_BE_MMIO | OHCI_QUIRK_BE_DESC;
        }
    }

    res_mem = platform_get_resource(op, IORESOURCE_MEM, 0);
    hcd->regs = devm_ioremap_resource(&op->dev, res_mem);
    if (IS_ERR(hcd->regs)) return PTR_ERR(hcd->regs);

    hcd->rsrc_start = res_mem->start;
    hcd->rsrc_len = resource_size(res_mem);

    err = usb_add_hcd(hcd, irq, 0);
    if (err) return err;

    platform_set_drvdata(op, hcd);

    return 0;
}

static int wiiu_ohci_remove(struct platform_device *dev) {
    struct usb_hcd *hcd = platform_get_drvdata(dev);

    usb_remove_hcd(hcd);
    usb_put_hcd(hcd);

    return 0;
}

static const struct of_device_id wiiu_ohci_ids[] = {
    { .compatible = "nintendo,ohci-wiiu", },
    {},
};
MODULE_DEVICE_TABLE(of, wiiu_ohci_ids);

static const struct platform_device_id wiiu_ohci_platform_table[] = {
    { "ohci-wiiu", 0 },
    {},
};
MODULE_DEVICE_TABLE(platform, wiiu_ohci_platform_table);

static struct platform_driver wiiu_ohci_driver = {
    .driver = {
        .name = DRV_MODULE_NAME,
        .of_match_table = wiiu_ohci_ids,
    },
    .id_table = wiiu_ohci_platform_table,
    .probe = wiiu_ohci_probe,
    .remove = wiiu_ohci_remove,
    .shutdown = usb_hcd_platform_shutdown,
};
