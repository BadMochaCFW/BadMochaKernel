/*
 * arch/powerpc/platforms/embedded6xx/wiiu/espresso-pic.c
 *
 * Nintendo Wii U "Espresso" interrupt controller support
 * Copyright (C) 2018 Ash Logan <quarktheawesome@gmail.com>
 * Copyright (C) 2018 Roberto Van Eeden <rwrr0644@gmail.com>
 *
 * Based on flipper-pic.c
 * Copyright (C) 2004-2009 The GameCube Linux Team
 * Copyright (C) 2007,2008,2009 Albert Herranz
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#define DRV_MODULE_NAME "espresso-pic"
#define pr_fmt(fmt) DRV_MODULE_NAME ": " fmt

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <asm/io.h>
#include "espresso-pic.h"

static DEFINE_PER_CPU(espresso_pic_t *, espresso_pic_cpu);

/* IRQ chip operations
 */

static void espresso_pic_mask_and_ack(struct irq_data* d) {
	espresso_pic_t *pic = *this_cpu_ptr(&espresso_pic_cpu);
	u32 mask = 1 << irqd_to_hwirq(d);
	out_be32(&pic->icr, mask);
	clrbits32(&pic->imr, mask);
}

static void espresso_pic_ack(struct irq_data* d) {
	espresso_pic_t *pic = *this_cpu_ptr(&espresso_pic_cpu);
	u32 mask = 1 << irqd_to_hwirq(d);
	out_be32(&pic->icr, mask);
}

static void espresso_pic_mask(struct irq_data* d) {
	espresso_pic_t *pic = *this_cpu_ptr(&espresso_pic_cpu);
	u32 mask = 1 << irqd_to_hwirq(d);
	clrbits32(&pic->imr, mask);
}

static void espresso_pic_unmask(struct irq_data* d) {
	espresso_pic_t *pic = *this_cpu_ptr(&espresso_pic_cpu);
	u32 mask = 1 << irqd_to_hwirq(d);
	setbits32(&pic->imr, mask);
}

static struct irq_chip espresso_pic_chip = {
	.name			= "espresso-pic",
	.irq_ack		= espresso_pic_ack,
	.irq_mask_ack	= espresso_pic_mask_and_ack,
	.irq_mask		= espresso_pic_mask,
	.irq_unmask		= espresso_pic_unmask,
};

/* Domain Ops
 */

static int espresso_pic_match(struct irq_domain *h, struct device_node *node, enum irq_domain_bus_token bus_token) {
	if (h->fwnode == &node->fwnode) {
		pr_debug("espresso-pic: %s IRQ matches with this driver\n", node->name);
		return 1;
	}
	return 0;
}

static int espresso_pic_alloc(struct irq_domain *h, unsigned int virq, unsigned int nr_irqs, void *arg) {
	//hacky; but it works. Brilliantly.
	unsigned int i;
	struct irq_fwspec* fwspec = arg;
	irq_hw_number_t hwirq = fwspec->param[0];

	for (i = 0; i < nr_irqs; i++) {
		irq_set_chip_data(virq + i, h->host_data);
		irq_set_status_flags(virq + i, IRQ_LEVEL);
		irq_set_chip_and_handler(virq + i, &espresso_pic_chip, handle_level_irq);
		//Here's the other end of that wonderful hack
		irq_domain_set_hwirq_and_chip(h, virq + i, hwirq + i, &espresso_pic_chip, h->host_data);
	}
	return 0;
}

static void espresso_pic_free(struct irq_domain *h, unsigned int virq, unsigned int nr_irqs) {
	pr_debug("espresso-pic: free\n");
}

const struct irq_domain_ops espresso_pic_ops = {
	.match = espresso_pic_match,
	.alloc = espresso_pic_alloc,
	.free = espresso_pic_free,
};

/* Determinate if there are interrupts pending
 */

//Store irq domain for espresso_pic_get_irq (the function gets no arguments)
static struct irq_domain *espresso_irq_domain;

unsigned int espresso_pic_get_irq(void)
{
	espresso_pic_t *pic = *this_cpu_ptr(&espresso_pic_cpu);
	u32 irq_status, irq;

	irq_status = in_be32(&pic->icr) & in_be32(&pic->imr);

	if (irq_status == 0)
		return 0;	//No IRQs pending

	//Find the first IRQ
	irq = __ffs(irq_status);

	//Return the virtual IRQ
	return irq_linear_revmap(espresso_irq_domain, irq);
}

/* Init function
 */
void __init espresso_pic_init(void) {
	struct device_node* np = of_find_compatible_node(NULL, NULL, "nintendo,espresso-pic");
	struct irq_domain* host;
	struct resource res;
	void __iomem *regbase;
	unsigned cpu;

	//This pic is needed
	BUG_ON(!np);

	//Map registers
	BUG_ON(of_address_to_resource(np, 0, &res) != 0);
	regbase = ioremap(res.start, resource_size(&res));
	BUG_ON(IS_ERR(regbase));

	for_each_present_cpu(cpu) {
		espresso_pic_t **pic = per_cpu_ptr(&espresso_pic_cpu, cpu);
		
		//Compute pic address
		*pic = regbase + (sizeof(espresso_pic_t) * cpu);

		//Mask and Ack CPU IRQs
		out_be32(&(*pic)->imr, 0);
		out_be32(&(*pic)->icr, 0xFFFFFFFF);

		pr_info("espresso pic for cpu %u at %08X\n", cpu, (unsigned)*pic);
	}

	//Register the PIC
	host = irq_domain_add_linear(np, ESPRESSO_NR_IRQS, &espresso_pic_ops, NULL);
	BUG_ON(!host);

	//Save irq domain for espresso_pic_get_irq
	espresso_irq_domain = host;

	irq_set_default_host(host);

	of_node_put(np);
}
