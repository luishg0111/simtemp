/*SPDX-License-Identifier: GPL-2.0-only*/ 
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
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/of.h>

#include "simtemp_core.h"

#include "simtemp_hrtimer.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define DRIVER_NAME "nxp_simtemp"
#define DEVICE_NAME "simtemp"
#define COMPATIBLE_NAME "nxp,simtemp"

#define RING_BUFF_SIZE 128
/*******************************************************************************
 * Types
 ******************************************************************************/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static ssize_t simtemp_read(struct file *file, char __user *buf,
                size_t count, loff_t *ppos);
static int simtemp_probe(struct platform_device *pdev);
static void simtemp_remove(struct platform_device *pdev);

static int __init simtemp_init_module(void);
static void __exit simtemp_exit_module(void);

/*******************************************************************************
 * Variables
 ******************************************************************************/
static struct simtemp_data *g_dev;
static struct platform_device *pdev_fake;

/* Device tree binding */
static const struct of_device_id nxp_simtemp_dt_ids[] = {
    { .compatible = COMPATIBLE_NAME },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, nxp_simtemp_dt_ids);

static const struct file_operations simtemp_fops = {
	.owner = THIS_MODULE,
	.read = simtemp_read,
	/* poll and ioctl left for next steps */
};

/* Platform driver registration */
static struct platform_driver simtemp_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = nxp_simtemp_dt_ids,
    },
    .probe = simtemp_probe,
    .remove = simtemp_remove,
};

/*******************************************************************************
 * Code
 ******************************************************************************/

/* minimal read: return one sample (binary) if available */
static ssize_t simtemp_read(struct file *file, char __user *buf,
                            size_t count, loff_t *ppos)
{
	struct simtemp_sample sample;
	int ret;

	if (count < sizeof(sample))
		return -EINVAL;

	ret = ring_pop_sample(g_dev, &sample);
	if (ret)
		return 0; /* EOF / no data */

	if (copy_to_user(buf, &sample, sizeof(sample)))
		return -EFAULT;

	return sizeof(sample);
}

/**
 * @brief Platform driver probe function
 * 
 * @param pdev 
 * @return int 
 */
static int simtemp_probe(struct platform_device *pdev)
{
	int ret = 0;
	u32 sampling_ms = NXP_SIMTEMP_DEFAULT_SAMPLING_MS;
	u32 threshold = NXP_SIMTEMP_DEFAULT_THRESHOLD_MILLIC;

	dev_info(&pdev->dev, "%s: probe start\n", DRIVER_NAME);

	/* allocate device context (managed) */
	g_dev = devm_kzalloc(&pdev->dev, sizeof(*g_dev), GFP_KERNEL);
	if (!g_dev)
		return -ENOMEM;

	g_dev->dev = &pdev->dev;

	/* read DT properties if present */
	if (pdev->dev.of_node) {
		if (of_property_read_u32(pdev->dev.of_node, "sampling-ms", &sampling_ms))
			dev_warn(&pdev->dev, "sampling-ms not specified, using default %u ms\n", sampling_ms);
		if (of_property_read_u32(pdev->dev.of_node, "threshold-mC", &threshold))
			dev_warn(&pdev->dev, "threshold-mC not specified, using default %u mC\n", threshold);
	}

	/* initialize fields */
	spin_lock_init(&g_dev->lock);
	g_dev->head = g_dev->tail = g_dev->count = 0;
	g_dev->temp_mC = 42000; /* start ~42.0°C */
	g_dev->threshold_mC = threshold;
	g_dev->total_samples = 0;

	/* set period (convert ms -> ns) */
	g_dev->period = ktime_set(0, (s64)sampling_ms * 1000000LL);

	/* misc device registration to create /dev/simtemp */
	g_dev->miscdev.minor = MISC_DYNAMIC_MINOR;
	g_dev->miscdev.name = DEVICE_NAME;
	g_dev->miscdev.fops = &simtemp_fops;
	ret = misc_register(&g_dev->miscdev);
	if (ret) {
		dev_err(&pdev->dev, "failed to register misc device: %d\n", ret);
		return ret;
	}

	/* initialize and start hrtimer */
	hrtimer_init(&g_dev->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	g_dev->timer.function = simtemp_timer_callback;
	hrtimer_start(&g_dev->timer, g_dev->period, HRTIMER_MODE_REL);

	dev_info(&pdev->dev, "%s: started sampling every %u ms, threshold=%u mC\n",
	         DRIVER_NAME, sampling_ms, g_dev->threshold_mC);

	return 0;
}
/**
 * @brief Platform driver remove function
 * 
 * @param pdev 
 */
static void simtemp_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s: remove - stopping\n", DRIVER_NAME);

	if (!g_dev) {
		dev_warn(&pdev->dev, "no device context\n");
		return;
	}

	/* cancel timer (wait for running callbacks to finish) */
	if (hrtimer_cancel(&g_dev->timer))
		dev_info(&pdev->dev, "timer cancelled\n");

	/* deregister misc device */
	misc_deregister(&g_dev->miscdev);

	dev_info(&pdev->dev, "%s: stopped after %llu samples (buffered=%u)\n",
	         DRIVER_NAME, (unsigned long long)g_dev->total_samples, g_dev->count);
}

/* module init/exit */
static int __init simtemp_init_module(void)
{
	int ret;
    struct platform_device_info pdevinfo = {
        .name = "nxp_simtemp", /* must match .compatible in your DT table */
        .id = -1,
    };

    pr_info("nxp_simtemp: registering fake platform device\n");

    /* Create a fake platform_device */
    pdev_fake = platform_device_register_full(&pdevinfo);
    if (IS_ERR(pdev_fake))
        return PTR_ERR(pdev_fake);

    /* Register the platform_driver (your main driver) */
    ret = platform_driver_register(&simtemp_driver);
    if (ret) {
        platform_device_unregister(pdev_fake);
        return ret;
    }

    pr_info("nxp_simtemp: driver and device registered successfully\n");
    return 0;
}

static void __exit simtemp_exit_module(void)
{
	pr_info("nxp_simtemp: unregistering driver and device\n");
    platform_driver_unregister(&simtemp_driver);
    platform_device_unregister(pdev_fake);
}

module_init(simtemp_init_module);
module_exit(simtemp_exit_module);

/* METADATA */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Luis Hernández <luishg0111@gmail.com>");
MODULE_DESCRIPTION("Virtual temperature sensor driver");
MODULE_VERSION("0.2");