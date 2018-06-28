/*
 * arch/powerpc/platforms/embedded6xx/wiiu/latte-ahball-pic.c
 *
 * Nintendo Wii U "Latte" interrupt controller support.
 * Copyright (C) 2018 Ash Logan <quarktheawesome@gmail.com>
 * Copyright (C) 2018 Roberto Van Eeden <rwrr0644@gmail.com>
 *
 * Based on hlwd-pic.c
 * Copyright (C) 2009 The GameCube Linux Team
 * Copyright (C) 2009 Albert Herranz
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 */

#define DRV_MODULE_NAME "latte-ahball-pic"
#define pr_fmt(fmt) DRV_MODULE_NAME ": " fmt

#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/irqdomain.h>
#include <asm/io.h>
#include "latte-ahball-pic.h"

static DEFINE_PER_CPU(lt_pic_t *, lt_pic_cpu);

/* IRQ chip operations
 */

static void latte_ahball_pic_mask_and_ack(struct irq_data *d) {
	lt_pic_t *pic = *this_cpu_ptr(&lt_pic_cpu);
	u32 mask = 1 << irqd_to_hwirq(d);
	out_be32(&pic->ahball_icr, mask);
	clrbits32(&pic->ahball_imr, mask);
}

static void latte_ahball_pic_ack(struct irq_data *d) {
	lt_pic_t *pic = *this_cpu_ptr(&lt_pic_cpu);
	u32 mask = 1 << irqd_to_hwirq(d);
	out_be32(&pic->ahball_icr, mask);
}

static void latte_ahball_pic_mask(struct irq_data *d) {
	lt_pic_t *pic = *this_cpu_ptr(&lt_pic_cpu);
	u32 mask = 1 << irqd_to_hwirq(d);
	clrbits32(&pic->ahball_imr, mask);
}

static void latte_ahball_pic_unmask(struct irq_data *d) {
	lt_pic_t *pic = *this_cpu_ptr(&lt_pic_cpu);
	u32 mask = 1 << irqd_to_hwirq(d);
	setbits32(&pic->ahball_imr, mask);
}

static struct irq_chip latte_ahball_pic = {
	.name			= "latte-ahball-pic",
	.irq_ack		= latte_ahball_pic_ack,
	.irq_mask_ack	= latte_ahball_pic_mask_and_ack,
	.irq_mask		= latte_ahball_pic_mask,
	.irq_unmask		= latte_ahball_pic_unmask,
};

/* Domain Ops
 */

static int latte_ahball_pic_match(struct irq_domain *h, struct device_node *node, enum irq_domain_bus_token bus_token) {
	if (h->fwnode == &node->fwnode) {
		pr_debug("%s IRQ matches with this driver\n", node->name);
		return 1;
	}
	return 0;
}

static int latte_ahball_pic_alloc(struct irq_domain *h, unsigned int virq, unsigned int nr_irqs, void *arg) {
	//See espresso-pic for slight elaboration
	unsigned int i;
	struct irq_fwspec* fwspec = arg;
	irq_hw_number_t hwirq = fwspec->param[0];

	for (i = 0; i < nr_irqs; i++) {
		irq_set_chip_data(virq + i, h->host_data);
		irq_set_status_flags(virq + i, IRQ_LEVEL);
		irq_set_chip_and_handler(virq + i, &latte_ahball_pic, handle_level_irq);
		irq_domain_set_hwirq_and_chip(h, virq + i, hwirq + i, &latte_ahball_pic, h->host_data);
	}
	return 0;
}

static void latte_ahball_pic_free(struct irq_domain *h, unsigned int virq, unsigned int nr_irqs) {
	pr_debug("free\n");
}

const struct irq_domain_ops latte_ahball_pic_ops = {
	.match = latte_ahball_pic_match,
	.alloc = latte_ahball_pic_alloc,
	.free = latte_ahball_pic_free,
};

/* Determinate if there are interrupts pending
 */
unsigned int latte_ahball_pic_get_irq(struct irq_domain *h) {
	lt_pic_t *pic = *this_cpu_ptr(&lt_pic_cpu);
	u32 irq_status, irq;

	irq_status = in_be32(&pic->ahball_icr) & in_be32(&pic->ahball_imr);

	if (irq_status == 0)
		return 0;	//No IRQs pending

	//Find the first IRQ
	irq = __ffs(irq_status);

	//Return the virtual IRQ
	return irq_linear_revmap(h, irq);
}

/* Cascade IRQ handler
 */
static void latte_ahball_irq_cascade(struct irq_desc *desc) {
	struct irq_domain *irq_domain = irq_desc_get_handler_data(desc);
	struct irq_chip *chip = irq_desc_get_chip(desc);
	unsigned int virq;

	raw_spin_lock(&desc->lock);
	chip->irq_mask(&desc->irq_data); /* IRQ_LEVEL */
	raw_spin_unlock(&desc->lock);

	virq = latte_ahball_pic_get_irq(irq_domain);
	if (virq)
		generic_handle_irq(virq);
	else
		pr_err("spurious interrupt!\n");

	raw_spin_lock(&desc->lock);
	chip->irq_ack(&desc->irq_data); /* IRQ_LEVEL */
	if (!irqd_irq_disabled(&desc->irq_data) && chip->irq_unmask)
		chip->irq_unmask(&desc->irq_data);
	raw_spin_unlock(&desc->lock);
}

/* Init function
 */
void __init latte_ahball_pic_init(void) {
	struct device_node *np = of_find_compatible_node(NULL, NULL, "nintendo,latte-ahball-pic");
	struct irq_domain* host;
	struct resource res;
	int irq_cascade;
	void __iomem *regbase;
	unsigned cpu;

	//This pic is needed for all the devices, make sure it's valid
	BUG_ON(!np);
	BUG_ON(!of_get_property(np, "interrupts", NULL));

	//Map registers
	BUG_ON(of_address_to_resource(np, 0, &res));
	regbase = ioremap(res.start, resource_size(&res));
	BUG_ON(IS_ERR(regbase));

	for_each_present_cpu(cpu) {
		lt_pic_t **pic = per_cpu_ptr(&lt_pic_cpu, cpu);
		
		//Compute pic address
		*pic = regbase + (sizeof(lt_pic_t) * cpu);

		//Mask and Ack CPU IRQs
		out_be32(&(*pic)->ahball_imr, 0);
		out_be32(&(*pic)->ahball_icr, 0xFFFFFFFF);

		pr_info("latte pic for cpu %u at %08X\n", cpu, (unsigned)*pic);
	}

	//Register PIC
	host = irq_domain_add_linear(np, LATTE_AHBALL_NR_IRQS, &latte_ahball_pic_ops, NULL);
	BUG_ON(!host);

	//Setup cascade interrupt
	irq_cascade = irq_of_parse_and_map(np, 0);
	irq_set_chained_handler_and_data(irq_cascade, latte_ahball_irq_cascade, host);

	of_node_put(np);
}
