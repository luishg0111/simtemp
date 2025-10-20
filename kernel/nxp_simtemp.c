/*SPDX-License-Identifier: GPL-2.0-only*/ 
/**
 * @file nxp_simtemp.c
 * @author Luis Hernández <luishg0111@gmail.com>
 * @brief Virtual simulated temperature sensor driver
 * @version 0.1
 * @date 2025-10-15
 *
 * Copyright (C) 2025 Luis Hernández <luishg0111@gmail.com>
 */

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/string.h>

#include "nxp_simtemp.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define DRIVER_NAME "nxp_simtemp"
#define DEVICE_NAME "simtemp"

/*******************************************************************************
 * Types
 ******************************************************************************/
/* Internal device data */
struct simtemp_data {
    int temp_mC;           /* current temperature in milli-degrees C */
    int threshold_mC;      /* alert threshold (milli-deg C) */
    int sampling_ms;       /* sampling interval (ms) */
    struct miscdevice miscdev;
};

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static ssize_t simtemp_read(struct file *file, char __user *buf,
                size_t count, loff_t *ppos);
static int nxp_simtemp_probe(struct platform_device *pdev);
static void nxp_simtemp_remove(struct platform_device *pdev);

/*******************************************************************************
 * Variables
 ******************************************************************************/
static struct simtemp_data *g_dev;

static const struct file_operations simtemp_fops = {
    .owner = THIS_MODULE,
    .read = simtemp_read,
};

/* Device tree binding */
static const struct of_device_id nxp_simtemp_dt_ids[] = {
    { .compatible = "nxp,simtemp" },
    { /* sentinel */ }
};

/* Platform driver registration */
static struct platform_driver nxp_simtemp_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = nxp_simtemp_dt_ids,
    },
    .probe = nxp_simtemp_probe,
    .remove = nxp_simtemp_remove,
};

/*******************************************************************************
 * Code
 ******************************************************************************/
/* File operations */
static ssize_t simtemp_read(struct file *file, char __user *buf,
                size_t count, loff_t *ppos)
{
    const char *msg = "SimTemp: dummy date\n";

    /* TODO: implement real read */
    return simple_read_from_buffer(buf, count, ppos, msg, strlen(msg));
}

/* Platform driver probe/remove */
static int nxp_simtemp_probe(struct platform_device *pdev)
{
    int ret;

    pr_info("%s: probing virtual sensor\n", DRIVER_NAME);

    g_dev = devm_kzalloc(&pdev->dev, sizeof(*g_dev), GFP_KERNEL);
    if (!g_dev)
        return -ENOMEM;

    g_dev->temp_mC = 25000;
    g_dev->threshold_mC = 40000;
    g_dev->sampling_ms = 100;

    g_dev->miscdev.minor = MISC_DYNAMIC_MINOR;
    g_dev->miscdev.name = DEVICE_NAME;
    g_dev->miscdev.fops = &simtemp_fops;

    ret = misc_register(&g_dev->miscdev);
    if (ret) {
        dev_err(&pdev->dev, "failed to register misc device\n");
        return ret;
    }

    pr_info("%s: module loaded successfully\n", DRIVER_NAME);
    return 0;
}

static void nxp_simtemp_remove(struct platform_device *pdev)
{
    pr_info("%s: removing virtual sensor\n", DRIVER_NAME);
    misc_deregister(&g_dev->miscdev);
}

MODULE_DEVICE_TABLE(of, nxp_simtemp_dt_ids);

module_platform_driver(nxp_simtemp_driver);

/* METADATA */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Luis Hernández <luishg0111@gmail.com>");
MODULE_DESCRIPTION("Virtual temperature sensor driver");
MODULE_VERSION("0.1");