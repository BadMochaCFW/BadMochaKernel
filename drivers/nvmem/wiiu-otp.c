/*
 * Copyright (C) 2021 Emmanuel Gil Peyrot <linkmauve@linkmauve.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/io.h>
#include <linux/nvmem-provider.h>
#include <linux/platform_device.h>

#define HW_OTPCMD  0
#define HW_OTPDATA 4
#define OTP_READ   0x80000000

struct wiiu_otp_priv {
	void __iomem *regs;
};

static int wiiu_otp_reg_read(void *context,
			     unsigned int reg, void *_val, size_t bytes)
{
	struct wiiu_otp_priv *priv = context;
	u32 *val = _val;
	int words = bytes >> 2;
	unsigned int addr;

	while (words--) {
		addr = ((reg >> 2) & 0x1f) | ((reg << 1) & 0x700);
		iowrite32be(addr | OTP_READ, priv->regs + HW_OTPCMD);
		*val++ = ioread32be(priv->regs + HW_OTPDATA);
		reg += 4;
	}

	return 0;
}

static struct nvmem_config econfig = {
	.name = "wiiu-otp",
	.size = 8 * 128,
	.stride = 4,
	.word_size = 4,
	.reg_read = wiiu_otp_reg_read,
	.read_only = true,
	.root_only = true,
};

static int wiiu_otp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct nvmem_device *nvmem;
	struct wiiu_otp_priv *priv;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	priv->regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(priv->regs))
		return PTR_ERR(priv->regs);

	econfig.dev = dev;
	econfig.priv = priv;

	nvmem = devm_nvmem_register(dev, &econfig);

	return PTR_ERR_OR_ZERO(nvmem);
}

static const struct of_device_id wiiu_otp_of_match[] = {
	{ .compatible = "nintendo,latte-otp", },
	{/* sentinel */},
};
MODULE_DEVICE_TABLE(of, wiiu_otp_of_match);

static struct platform_driver wiiu_otp_driver = {
	.probe = wiiu_otp_probe,
	.driver = {
		.name = "nintendo,latte-otp",
		.of_match_table = wiiu_otp_of_match,
	},
};
module_platform_driver(wiiu_otp_driver);
MODULE_AUTHOR("Emmanuel Gil Peyrot <linkmauve@linkmauve.fr>");
MODULE_DESCRIPTION("Nintendo Wii U OTP driver");
MODULE_LICENSE("GPL v2");
