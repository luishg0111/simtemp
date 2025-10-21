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
#include "simtemp_char.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Types
 ******************************************************************************/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static int simtemp_probe(struct platform_device *pdev);
static void simtemp_remove(struct platform_device *pdev);

/*******************************************************************************
 * Variables
 ******************************************************************************/

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

module_platform_driver(simtemp_driver);
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
	u32 sampling_ms = SIMTEMP_DEFAULT_SAMPLING_MS;
	int ret;

	sdat = devm_kzalloc(&pdev->dev, sizeof(*sdat), GFP_KERNEL);
	if (!sdat)
	{
		return -ENOMEM;
	}

	sdat->dev = &pdev->dev;

	/* Optional DT: read sampling-ms / threshold-mC if present */
	if (pdev->dev.of_node) {
		of_property_read_u32(pdev->dev.of_node, "sampling-ms", &sampling_ms);
		of_property_read_u32(pdev->dev.of_node, "threshold-mC", &sdat->threshold_mC);
	}

	/* init hrtimer + ring + state */
	ret = simtemp_hrtimer_init(sdat, sampling_ms);
	if (ret)
		return ret;

	/* expose /dev/simtemp */
	ret = simtemp_char_init(sdat);
	if (ret) {
		simtemp_hrtimer_exit(sdat);
		return ret;
	}

	platform_set_drvdata(pdev, sdat);
	dev_info(&pdev->dev, "nxp_simtemp: probe ok (sampling=%u ms)\n", sdat->sampling_ms);
	return 0;
}

/**
 * @brief Platform driver remove function
 * 
 * @param pdev 
 */
static void simtemp_remove(struct platform_device *pdev)
{
	struct simtemp_data *sdat = platform_get_drvdata(pdev);
	simtemp_char_exit(sdat);
	simtemp_hrtimer_exit(sdat);
	dev_info(&pdev->dev, "nxp_simtemp: removed\n");
}


/* METADATA */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Luis Hernández <luishg0111@gmail.com>");
MODULE_DESCRIPTION("Virtual temperature sensor driver");
MODULE_VERSION("0.3");