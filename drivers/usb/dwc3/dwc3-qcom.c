/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/usb/phy.h>

#include "core.h"


struct dwc3_qcom {
	struct device		*dev;

	struct clk		*core_clk;
	struct clk		*iface_clk;
	struct clk		*sleep_clk;

	struct regulator	*gdsc;
};

static int dwc3_qcom_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct dwc3_qcom *mdwc;
	int ret = 0;

	mdwc = devm_kzalloc(&pdev->dev, sizeof(*mdwc), GFP_KERNEL);
	if (!mdwc)
		return -ENOMEM;

	platform_set_drvdata(pdev, mdwc);

	mdwc->dev = &pdev->dev;

	mdwc->gdsc = devm_regulator_get(mdwc->dev, "gdsc");

	mdwc->core_clk = devm_clk_get(mdwc->dev, "core");
	if (IS_ERR(mdwc->core_clk)) {
		dev_dbg(mdwc->dev, "failed to get core clock\n");
		return PTR_ERR(mdwc->core_clk);
	}

	mdwc->iface_clk = devm_clk_get(mdwc->dev, "iface");
	if (IS_ERR(mdwc->iface_clk)) {
		dev_dbg(mdwc->dev, "failed to get iface clock, skipping\n");
		mdwc->iface_clk = NULL;
	}

	mdwc->sleep_clk = devm_clk_get(mdwc->dev, "sleep");
	if (IS_ERR(mdwc->sleep_clk)) {
		dev_dbg(mdwc->dev, "failed to get sleep clock, skipping\n");
		mdwc->sleep_clk = NULL;
	}

	if (!IS_ERR(mdwc->gdsc)) {
		ret = regulator_enable(mdwc->gdsc);
		if (ret)
			dev_err(mdwc->dev, "cannot enable gdsc\n");
	}

	clk_prepare_enable(mdwc->core_clk);

	if (mdwc->iface_clk)
		clk_prepare_enable(mdwc->iface_clk);

	if (mdwc->sleep_clk)
		clk_prepare_enable(mdwc->sleep_clk);

	ret = of_platform_populate(node, NULL, NULL, mdwc->dev);
	if (ret) {
		dev_err(mdwc->dev, "failed to register core - %d\n", ret);
		dev_dbg(mdwc->dev, "failed to add create dwc3 core\n");
		goto dis_clks;
	}

	return 0;

dis_clks:

	dev_err(mdwc->dev, "disabling clocks\n");

	if (mdwc->sleep_clk)
		clk_disable_unprepare(mdwc->sleep_clk);

	if (mdwc->iface_clk)
		clk_disable_unprepare(mdwc->iface_clk);

	clk_disable_unprepare(mdwc->core_clk);

	if (!IS_ERR(mdwc->gdsc)) {
		ret = regulator_disable(mdwc->gdsc);
		if (ret)
			dev_dbg(mdwc->dev, "cannot disable gdsc\n");
	}

	return ret;
}

static int dwc3_qcom_remove(struct platform_device *pdev)
{
	int ret = 0;

	struct dwc3_qcom *mdwc = platform_get_drvdata(pdev);

	if (mdwc->sleep_clk)
		clk_disable_unprepare(mdwc->sleep_clk);

	if (mdwc->iface_clk)
		clk_disable_unprepare(mdwc->iface_clk);

	clk_disable_unprepare(mdwc->core_clk);

	if (!IS_ERR(mdwc->gdsc)) {
		ret = regulator_disable(mdwc->gdsc);
		if (ret)
			dev_dbg(mdwc->dev, "cannot disable gdsc\n");
	}
	return ret;
}

static const struct of_device_id of_dwc3_match[] = {
	{ .compatible = "qcom,dwc3" },
	{ /* Sentinel */ }
};
MODULE_DEVICE_TABLE(of, of_dwc3_match);

static struct platform_driver dwc3_qcom_driver = {
	.probe		= dwc3_qcom_probe,
	.remove		= dwc3_qcom_remove,
	.driver		= {
		.name	= "qcom-dwc3",
		.owner	= THIS_MODULE,
		.of_match_table	= of_dwc3_match,
	},
};

module_platform_driver(dwc3_qcom_driver);

MODULE_ALIAS("platform:qcom-dwc3");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DesignWare USB3 QCOM Glue Layer");
