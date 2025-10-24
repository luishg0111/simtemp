// SPDX-License-Identifier: GPL-2.0-only
/**
 * @file simtemp_core.c
 * @author Luis Hernández <luishg0111@gmail.com>
 * @brief Virtual simulated temperature sensor driver
 * @version 0.1
 * @date 2025-10-15
 *
 * @copyright Copyright (C) 2025 Luis Hernández <luishg0111@gmail.com>
 *
 */
/*******************************************************************************
 * Includes
 ******************************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/ktime.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/version.h>

#include "simtemp_core.h"

#include "simtemp_hrtimer.h"
#include "simtemp_char.h"
#include "simtemp_sysfs.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Types
 ******************************************************************************/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static int __init simtemp_init_module(void);
static void __exit simtemp_exit_module(void);

static int simtemp_probe(struct platform_device *pdev);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 5, 0)
static int simtemp_remove(struct platform_device *pdev);
#else
static void simtemp_remove(struct platform_device *pdev);
#endif

static int simtemp_parse_dt(struct simtemp_device *sdev, struct device *dev);
static const char *simtemp_mode_to_str(enum simtemp_mode mode);
/*******************************************************************************
 * Variables
 ******************************************************************************/
/* Global pointer for the manually created platform device */
static struct platform_device *simtemp_pdev;

/* Device tree binding */
static const struct of_device_id simtemp_dt_ids[] = {
	{ .compatible = COMPATIBLE_NAME },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, simtemp_dt_ids);

/* Platform driver registration */
static struct platform_driver simtemp_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = simtemp_dt_ids,
	},
	.probe = simtemp_probe,
	.remove = simtemp_remove,
};

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * @brief Device Tree parsing helper
 *
 * @param sdev
 * @param dev
 * @return int
 */
static int simtemp_parse_dt(struct simtemp_device *sdev, struct device *dev)
{
	struct device_node *np = dev->of_node;
	u32 samptemp;
	s32 threstemp;
	enum simtemp_mode modetemp;

	if (!np) {
		simtemp_dbg(dev, "no Device Tree node found, using defaults\n");
		sdev->sampling_ms		= SIMTEMP_DEFAULT_SAMPLING_MS;
		sdev->threshold_mc		= SIMTEMP_DEFAULT_THRESHOLD_MILLIC;
		sdev->mode			= SIMTEMP_DEFAULT_MODE;
		/* Initialize stats members */
		sdev->stats.alerts_count	= 0;
		sdev->stats.errors_count	= 0;
		sdev->stats.updates_count	= 0;
		sdev->stats.alert_pending	= false;
		return 0;
	}

	simtemp_dbg(dev, "parsing Device Tree properties\n");

	if (!of_property_read_u32(np, "sampling-ms", &samptemp)) {
		sdev->sampling_ms = samptemp;
	} else {
		sdev->sampling_ms = SIMTEMP_DEFAULT_SAMPLING_MS;
		dev_warn(dev, "sampling-ms not found, using default %u ms\n",
			 sdev->sampling_ms);
	}

	if (!of_property_read_s32(np, "threshold-mC", &threstemp)) {
		sdev->threshold_mc = threstemp;
	} else {
		sdev->threshold_mc = SIMTEMP_DEFAULT_THRESHOLD_MILLIC;
		dev_warn(dev, "threshold-mC not found, using default %d mC\n",
			 sdev->threshold_mc);
	}

	if (!of_property_read_u32(np, "mode", (u32 *)&modetemp)) {
		sdev->mode = modetemp;
	} else {
		sdev->mode = SIMTEMP_DEFAULT_MODE;
		dev_warn(dev, "mode not found, using default %s\n",
			 simtemp_mode_to_str(sdev->mode));
	}


	simtemp_dbg(dev, "DT config: sampling=%u ms, threshold=%u mC, mode=%s\n",
		 sdev->sampling_ms, sdev->threshold_mc, simtemp_mode_to_str(sdev->mode));

	return 0;
}

/**
 * @brief Parse simtemp_mode to string
 *
 * @param mode
 * @return const char*
 */
static const char *simtemp_mode_to_str(enum simtemp_mode mode)
{
	switch (mode) {
	case NORMAL:
		return "normal";
	case NOISY:
		return "noisy";
	case RAMP:
		return "ramp";
	default:
		return "unknown";
	}
}

/**
 * @brief Platform driver probe function
 *
 * @param pdev
 * @return int
 */
static int simtemp_probe(struct platform_device *pdev)
{
	struct simtemp_device *sdev;
	int ret;

	dev_info(&pdev->dev, "Compatible driver detected. Initializing resources...\n");

	sdev = devm_kzalloc(&pdev->dev, sizeof(*sdev), GFP_KERNEL);
	if (!sdev)
		return -ENOMEM;

	sdev->dev = &pdev->dev;
	/* store pointer for remove path */
	platform_set_drvdata(pdev, sdev);

	/* Parse DT or defaults */
	ret = simtemp_parse_dt(sdev, &pdev->dev);
	if (ret)
		return ret;

	/* Initialize modules */
	ret = simtemp_hrtimer_init(sdev, sdev->sampling_ms);
	if (ret) {
		dev_err(&pdev->dev, "%s: failed to init hrtimer (%d)\n", DRIVER_NAME, ret);
		return ret; /* devm allocation -> no explicit free */
	}

	ret = simtemp_char_init(sdev);
	if (ret) {
		dev_err(&pdev->dev, "%s: failed to init char device (%d)\n", DRIVER_NAME, ret);
		simtemp_hrtimer_exit(sdev);
		return ret;
	}

	ret = simtemp_sysfs_init(sdev);
	if (ret) {
		dev_err(&pdev->dev, "%s: failed to create sysfs attributes (%d)\n",
			DRIVER_NAME, ret);
		simtemp_char_exit(sdev);
		return ret;
	}
	dev_info(&pdev->dev, "%s: Loaded successfully\n", DRIVER_NAME);

	simtemp_dbg(sdev->dev, "%s: (sampling=%u ms, threshold=%d mC, mode=%s)\n",
		 DRIVER_NAME, sdev->sampling_ms, sdev->threshold_mc,
		 simtemp_mode_to_str(sdev->mode));
	return ret;
}

/**
 * @brief Platform driver remove function
 *
 * @param pdev
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 5, 0)
static int simtemp_remove(struct platform_device *pdev)
{
	struct simtemp_device *sdev = platform_get_drvdata(pdev);

	simtemp_dbg(&pdev->dev, "%s: remove - cleaning up\n", DRIVER_NAME);

	/* Remove sysfs */
	simtemp_sysfs_exit(sdev);
	/* Remove char device */
	simtemp_char_exit(sdev);
	/* Remove hrtimer */
	simtemp_hrtimer_exit(sdev);

	dev_info(&pdev->dev, "%s: removed cleanly\n", DRIVER_NAME);
	return 0;
}
#else
static int simtemp_remove(struct platform_device *pdev)
{
	struct simtemp_device *sdev = platform_get_drvdata(pdev);

	simtemp_dbg(&pdev->dev, "%s: remove - cleaning up\n", DRIVER_NAME);

	/* Remove sysfs */
	simtemp_sysfs_exit(sdev);
	/* Remove char device */
	simtemp_char_exit(sdev);
	/* Remove hrtimer */
	simtemp_hrtimer_exit(sdev);

	simtemp_dbg(&pdev->dev, "%s: removed cleanly\n", DRIVER_NAME);
	return 0;
}
#endif

/**
 * @brief Module initialization
 *
 * @return int
 */
static int __init simtemp_init_module(void)
{
	int ret;

	/* Info the manually created platform device*/
	struct platform_device_info pdevinfo = {
		.name = "nxp_simtemp", /* must match driver name*/
		.id = PLATFORM_DEVID_NONE,
	};

	simtemp_pr_dbg("%s: registering manual platform device\n", DRIVER_NAME);
	ret = platform_driver_register(&simtemp_driver);
	if (ret)
		return ret;

	/*create a manual platform device so probe() runs even without DT */
	simtemp_pdev = platform_device_register_full(&pdevinfo);
	if (IS_ERR(simtemp_pdev)) {
		pr_err("%s: failed to register platform device\n", DRIVER_NAME);
		platform_driver_unregister(&simtemp_driver);
		return PTR_ERR(simtemp_pdev);
	}

	simtemp_pr_dbg("%s: platform driver registered successfully\n", DRIVER_NAME);

	return ret;
}

/**
 * @brief Module exit
 *
 */
static void __exit simtemp_exit_module(void)
{
	simtemp_pr_dbg("%s: unregistering driver and device\n", DRIVER_NAME);

	if (simtemp_pdev) {
		platform_device_unregister(simtemp_pdev);
		simtemp_pdev = NULL;
	}
	platform_driver_unregister(&simtemp_driver);

	simtemp_pr_dbg("%s: module exit complete\n", DRIVER_NAME);
}

module_init(simtemp_init_module);
module_exit(simtemp_exit_module);

/* Module Metadata */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Luis Hernández <luishg0111@gmail.com>");
MODULE_DESCRIPTION("Virtual temperature sensor driver");
MODULE_VERSION("0.4");
