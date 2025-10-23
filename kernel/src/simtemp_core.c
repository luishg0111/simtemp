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
static int simtemp_remove(struct platform_device *pdev);
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
 * @brief Platform driver probe function
 *
 * @param pdev
 * @return int
 */
static int simtemp_probe(struct platform_device *pdev)
{
	struct simtemp_data *sdat;
	int ret = 0;

	dev_info(&pdev->dev, "Compatible driver detected. Initializing resources...\n");

	sdat = devm_kzalloc(&pdev->dev, sizeof(*sdat), GFP_KERNEL);
	if (!sdat)
		return -ENOMEM;

	platform_set_drvdata(pdev, sdat);
	sdat->dev = &pdev->dev;

	/* Optional DT: read sampling-ms / threshold-mC if present */
	if (pdev->dev.of_node) {
		of_property_read_u32(pdev->dev.of_node, "sampling-ms", &sdat->last_sample.sampling_ms);
		of_property_read_s32(pdev->dev.of_node, "threshold-mC", &sdat->last_sample.threshold_mc);
	}
	
	/* ensure sensible defaults if DT helper left them zero/uninitialized */
	if (sdat->last_sample.sampling_ms == 0)
		sdat->last_sample.sampling_ms = SIMTEMP_DEFAULT_SAMPLING_MS;

	if (sdat->last_sample.threshold_mc == 0)
		sdat->last_sample.threshold_mc = SIMTEMP_DEFAULT_THRESHOLD_MILLIC;
	
	if (sdat->last_sample.temp_mc == 0)
		sdat->last_sample.temp_mc = SIMTEMP_DEFAULT_TEMPERATURE_MC;

	if (sdat->last_sample.timestamp_ns == 0)
		sdat->last_sample.timestamp_ns = SIMTEMP_DEFAULT_TIMESTAMP_NS;

	spin_lock_init(&sdat->rb.lock);
	init_waitqueue_head(&sdat->read_queue);

	/* Create dedicated workqueue */
	sdat->wq = alloc_workqueue("simtemp_wq", WQ_UNBOUND, 1);
	//INIT_WORK(&sdat->work, wake_up_interruptible(&sdat->read_queue));

	/* store sampling period in ktime in hr timer init, submodule will use sampling_ms */
	dev_info(&pdev->dev, "%s: config sampling_ms=%u threshold_mc=%d\n",
		 DRIVER_NAME, sdat->last_sample.sampling_ms, sdat->last_sample.threshold_mc);

	/* 1) Initialize sampling engine (hrtimer) */
	ret = simtemp_hrtimer_init(sdat, sdat->last_sample.sampling_ms);
	if (ret) {
		dev_err(&pdev->dev, "%s: failed to init hrtimer (%d)\n", DRIVER_NAME, ret);
		return ret; /* devm allocation -> no explicit free */
	}

	/* 2) Initialize char device (/dev/simtemp) */
	ret = simtemp_char_init(sdat);
	if (ret) {
		dev_err(&pdev->dev, "%s: failed to init char device (%d)\n", DRIVER_NAME, ret);
		simtemp_hrtimer_exit(sdat);
	}

	/* 3) Initialize sysfs attributes */
	ret = simtemp_sysfs_init(sdat);
	if (ret) {
		dev_err(&pdev->dev, "%s: failed to create sysfs attributes (%d)\n",
			DRIVER_NAME, ret);
		simtemp_char_exit(sdat);
	}

	/* store pointer for remove path */
	platform_set_drvdata(pdev, sdat);

	dev_info(&pdev->dev, "%s: probe successful\n (sampling=%u ms)\n", DRIVER_NAME,
		 sdat->last_sample.sampling_ms);
	return ret;
}

/**
 * @brief Platform driver remove function
 *
 * @param pdev
 */
static int simtemp_remove(struct platform_device *pdev)
{
	struct simtemp_data *sdat = platform_get_drvdata(pdev);

	cancel_work_sync(&sdat->work);
	destroy_workqueue(sdat->wq);

	/* Remove sysfs */
	simtemp_sysfs_exit(sdat);

	/* Remove char device */
	simtemp_char_exit(sdat);

	/* Remove hrtimer */
	simtemp_hrtimer_exit(sdat);

	dev_info(&pdev->dev, "%s: removed cleanly\n", DRIVER_NAME);

	return 0;
}

/**
 * @brief Module initialization
 *
 * @return int
 */
static int __init simtemp_init_module(void)
{
	int ret = 0;

	struct platform_device_info pdevinfo = {
		.name = "nxp_simtemp", /* must match .compatible in your DT table */
		.id = -1,
	};

	pr_info("%s: registering manual platform device\n", DRIVER_NAME);

	/* Register the platform_driver (your main driver) */
	ret = platform_driver_register(&simtemp_driver);
	if (ret) {
		pr_err("%s: failed to register platform driver: %d\n", DRIVER_NAME, ret);
		return ret;
	}

	/* Create a fake platform_device */
	simtemp_pdev = platform_device_register_full(&pdevinfo);
	if (IS_ERR(simtemp_pdev)) {
		pr_err("%s: failed to register platform device\n", DRIVER_NAME);
		platform_driver_unregister(&simtemp_driver);
		return PTR_ERR(simtemp_pdev);
	}

	pr_info("%s: platform driver registered successfully\n", DRIVER_NAME);

	return ret;
}

/**
 * @brief Module exit
 *
 */
static void __exit simtemp_exit_module(void)
{
	pr_info("%s: unregistering driver and device\n", DRIVER_NAME);
	platform_driver_unregister(&simtemp_driver);
	platform_device_unregister(simtemp_pdev);
}

module_init(simtemp_init_module);
module_exit(simtemp_exit_module);

/* Module Metadata */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Luis Hernández <luishg0111@gmail.com>");
MODULE_DESCRIPTION("Virtual temperature sensor driver");
MODULE_VERSION("0.4");
