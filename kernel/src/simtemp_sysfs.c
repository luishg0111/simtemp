// SPDX-License-Identifier: GPL-2.0-only
/**
 * @file simtemp_sysfs.c
 * @author Luis Hernández <luishg0111@gmail.com>
 * @brief Sysfs interface for simulated temperature sensor
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
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/string.h>

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
static ssize_t sampling_ms_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t sampling_ms_store(struct device *dev, struct device_attribute *attr,
				 const char *buf, size_t count);
static ssize_t threshold_mc_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t threshold_mc_store(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count);
static ssize_t stats_show(struct device *dev, struct device_attribute *attr, char *buf);
/*******************************************************************************
 * Variables
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * @brief Show sampling_ms attribute
 *
 * @param dev
 * @param attr
 * @param buf
 * @return ssize_t
 */
static ssize_t sampling_ms_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct simtemp_data *sdat = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%u\n", sdat->sampling_ms);
}

/**
 * @brief Show sampling_ms attribute
 *
 * @param dev
 * @param attr
 * @param buf
 * @param count
 * @return ssize_t
 */
static ssize_t sampling_ms_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct simtemp_data *sdat = dev_get_drvdata(dev);
	u32 new_ms;
	int ret;

	ret = kstrtou32(buf, 0, &new_ms);
	if (ret)
		return ret;

	if (new_ms == 0 || new_ms > 10000)
		return -EINVAL;

	dev_info(sdat->dev, "sampling_ms updated to %u ms\n",
		 sdat->sampling_ms);

	return count;
}
static DEVICE_ATTR_RW(sampling_ms);

/**
 * @brief Show threshold_mc attribute
 *
 * @param dev
 * @param attr
 * @param buf
 * @return ssize_t
 */
static ssize_t threshold_mc_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct simtemp_data *sdat = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%u\n", sdat->threshold_mc);
}

/**
 * @brief Store threshold_mc attribute
 *
 * @param dev
 * @param attr
 * @param buf
 * @param count
 * @return ssize_t
 */
static ssize_t threshold_mc_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct simtemp_data *sdat = dev_get_drvdata(dev);
	u32 new_thr;
	int ret;

	ret = kstrtou32(buf, 0, &new_thr);
	if (ret)
		return ret;

	if (new_thr < -5000 || new_thr > 100000) {
		dev_warn(dev, "Threshold out of range (-5 to 100 C). Introduce valid value.\n");
		return -EINVAL;
	}

	spin_lock(&sdat->lock);
	sdat->threshold_mc = new_thr;
	spin_unlock(&sdat->lock);

	dev_info(sdat->dev, "threshold_mc updated to %u\n", sdat->threshold_mc);

	return count;
}
static DEVICE_ATTR_RW(threshold_mc);

/**
 * @brief Show mode attribute
 *
 * @param dev
 * @param attr
 * @param buf
 * @return ssize_t
 */
static ssize_t mode_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct simtemp_data *sdat = dev_get_drvdata(dev);
	const char *mode_str;

	switch (sdat->mode) {
	case NORMAL:
		mode_str = "normal\n";
		break;
	case NOISY:
		mode_str = "noisy\n";
		break;
	case RAMP:
		mode_str = "ramp\n";
		break;
	default:
		mode_str = "unknown\n";
		break;
	}

	return sprintf(buf, "%s", mode_str);
}

/**
 * @brief Store mode attribute
 *
 * @param dev
 * @param attr
 * @param buf
 * @param count
 * @return ssize_t
 */
static ssize_t mode_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct simtemp_data *sdat = dev_get_drvdata(dev);
	enum simtemp_mode new_mode;

	if (sysfs_streq(buf, "normal\n")) {
		new_mode = NORMAL;
	} else if (sysfs_streq(buf, "noisy\n")) {
		new_mode = NOISY;
	} else if (sysfs_streq(buf, "ramp\n")) {
		new_mode = RAMP;
	} else {
		dev_warn(dev, "Invalid mode. Use: normal|noisy|ramp\n");
		return -EINVAL;
	}

	spin_lock(&sdat->lock);
	sdat->mode = new_mode;
	spin_unlock(&sdat->lock);

	return count;
}
static DEVICE_ATTR_RW(mode);

/**
 * @brief Show stats attribute
 *
 * @param dev
 * @param attr
 * @param buf
 * @return ssize_t
 */
static ssize_t stats_show(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	struct simtemp_data *sdat = dev_get_drvdata(dev);

	return sysfs_emit(buf, "Updates: %lu\nAlerts: %lu\nErrors: %lu\n",
			  sdat->stats.update_count,
			  sdat->stats.alert_count,
			  sdat->stats.error_count);
}
static DEVICE_ATTR_RO(stats);

/* sysfs attributes */
static struct attribute *simtemp_attrs[] = {
	&dev_attr_sampling_ms.attr,
	&dev_attr_threshold_mc.attr,
	&dev_attr_mode.attr,
	&dev_attr_stats.attr,
	NULL,
};

static const struct attribute_group simtemp_attr_group = {
	.attrs = simtemp_attrs,
};

/**
 * @brief Initialize sysfs attributes
 *
 * @param sdat
 * @return int
 */
int simtemp_sysfs_init(struct simtemp_data *sdat)
{
	int ret;
	struct device *dev = sdat->dev;

	ret = sysfs_create_group(&dev->kobj, &simtemp_attr_group);
	if (ret) {
		dev_err(dev, "failed to create sysfs group\n");
		return ret;
	}
	dev_set_drvdata(dev, sdat);
	sdat->dev = dev;
	dev_info(dev, "sysfs attributes created under /sys/class/.../simtemp\n");

	return 0;
}
EXPORT_SYMBOL_GPL(simtemp_sysfs_init);

/**
 * @brief Clean up sysfs attributes
 *
 * @param sdat
 */
void simtemp_sysfs_exit(struct simtemp_data *sdat)
{
	if (sdat->dev)
		sysfs_remove_group(&sdat->dev->kobj, &simtemp_attr_group);
}
EXPORT_SYMBOL_GPL(simtemp_sysfs_exit);
