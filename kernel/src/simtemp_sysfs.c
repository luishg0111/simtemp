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
static ssize_t record_format_show(struct device *dev, struct device_attribute *attr, char *buf);

/*******************************************************************************
 * Variables
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/
/*
 * Locking policy:
 *  - simtemp_rb_push() and simtemp_rb_pop() take rb->lock internally.
 *  - simtemp_rb_has_data() is lock-free and safe for concurrent readers.
 *    Use READ_ONCE() to avoid reordering issues.
 */
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
	struct simtemp_device *sdev = dev_get_drvdata(dev);
	ssize_t ret;

	mutex_lock(&sdev->device_mutex);
	ret = sysfs_emit(buf, "%u\n", sdev->sampling_ms);
	mutex_unlock(&sdev->device_mutex);

	return ret;
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
	struct simtemp_device *sdev = dev_get_drvdata(dev);
	u32 new_ms;
	int ret;
	unsigned long devflags;

	ret = kstrtou32(buf, 0, &new_ms);
	if (ret)
		return ret;

	if (new_ms < SIMTEMP_SAMPLING_MS_MIN || new_ms > SIMTEMP_SAMPLING_MS_MAX) {
		dev_warn(dev, "sampling_ms (period) out of range (%u to %u ms).\n",
			 SIMTEMP_SAMPLING_MS_MIN, SIMTEMP_SAMPLING_MS_MAX);
		return -EINVAL;
	}

	/* protect concurrent access with hrtimer */
	spin_lock_irqsave(&sdev->device_lock, devflags);
	sdev->sampling_ms = new_ms;
	spin_unlock_irqrestore(&sdev->device_lock, devflags);

	/* reprogram timer outside lock */
	hrtimer_cancel(&sdev->timer);
	hrtimer_start(&sdev->timer, ms_to_ktime(READ_ONCE(sdev->sampling_ms)), HRTIMER_MODE_REL);

	simtemp_dbg(sdev->dev, "sampling_ms updated to %u ms\n",
		    sdev->sampling_ms);

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
	struct simtemp_device *sdev = dev_get_drvdata(dev);
	ssize_t ret;

	mutex_lock(&sdev->device_mutex);
	ret = sysfs_emit(buf, "%u\n", sdev->threshold_mc);
	mutex_unlock(&sdev->device_mutex);

	return ret;
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
	struct simtemp_device *sdev = dev_get_drvdata(dev);
	s32 new_thr;
	int ret;
	unsigned long devflags;

	ret = kstrtoint(buf, 0, &new_thr);
	if (ret)
		return ret;

	if (new_thr < SIMTEMP_TEMPERATURE_MC_MIN || new_thr > SIMTEMP_TEMPERATURE_MC_MAX) {
		dev_warn(dev, "Threshold out of range (%d to %d mC). Introduce valid value.\n",
			 SIMTEMP_TEMPERATURE_MC_MIN, SIMTEMP_TEMPERATURE_MC_MAX);
		return -EINVAL;
	}

	/* protect concurrent access with hrtimer */
	spin_lock_irqsave(&sdev->device_lock, devflags);
	sdev->threshold_mc = new_thr;
	spin_unlock_irqrestore(&sdev->device_lock, devflags);

	simtemp_dbg(sdev->dev, "threshold_mc updated to %d\n", sdev->threshold_mc);

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
	struct simtemp_device *sdev = dev_get_drvdata(dev);
	const char *mode_str;
	ssize_t ret;

	switch (sdev->mode) {
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

	mutex_lock(&sdev->device_mutex);
	ret = sysfs_emit(buf, "%s", mode_str);
	mutex_unlock(&sdev->device_mutex);

	return ret;
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
	struct simtemp_device *sdev = dev_get_drvdata(dev);
	enum simtemp_mode new_mode;

	if (sysfs_streq(buf, "normal")) {
		new_mode = NORMAL;
	} else if (sysfs_streq(buf, "noisy")) {
		new_mode = NOISY;
	} else if (sysfs_streq(buf, "ramp")) {
		new_mode = RAMP;
	} else {
		dev_warn(dev, "Invalid mode. Use: normal|noisy|ramp\n");
		return -EINVAL;
	}

	mutex_lock(&sdev->device_mutex);
	sdev->mode = new_mode;
	mutex_unlock(&sdev->device_mutex);

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
	struct simtemp_device *sdev = dev_get_drvdata(dev);
	ssize_t ret;

	mutex_lock(&sdev->device_mutex);
	ret = sysfs_emit(buf, "Updates: %lu\nAlerts: %lu\nErrors: %lu\n",
			 sdev->stats.updates_count,
			 sdev->stats.alerts_count,
			 sdev->stats.errors_count);
	mutex_unlock(&sdev->device_mutex);

	return ret;
}
static DEVICE_ATTR_RO(stats);

/**
 * @brief Binary format description for record_format attribute
 *
 * @param dev
 * @param attr
 * @param buf
 * @return ssize_t
 */
static ssize_t record_format_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	/* Describe the binary layout of struct simtemp_sample */
	return sysfs_emit(buf, "timestamp_ns:u64 temp_mC:s32 flags:u32\n");
}
static DEVICE_ATTR_RO(record_format);

/* sysfs attributes */
static struct attribute *simtemp_attrs[] = {
	&dev_attr_sampling_ms.attr,
	&dev_attr_threshold_mc.attr,
	&dev_attr_mode.attr,
	&dev_attr_stats.attr,
	&dev_attr_record_format.attr,
	NULL,
};

static const struct attribute_group simtemp_attr_group = {
	.attrs = simtemp_attrs,
};

/**
 * @brief Initialize sysfs attributes
 *
 * @param sdev
 * @return int
 */
int simtemp_sysfs_init(struct simtemp_device *sdev)
{
	int ret;

	/* Create global class */
	if (!sdev->cls) {
		sdev->cls = class_create(CLASS_NAME);
		if (IS_ERR(sdev->cls)) {
			pr_err("simtemp: failed to create class\n");
		return PTR_ERR(sdev->cls);
		}
	}

	/* Create device in class */
	sdev->dev = device_create(sdev->cls, NULL, 0, sdev,
				  "%s%d", DEVICE_NAME, 0);

	if (IS_ERR(sdev->dev)) {
		ret = PTR_ERR(sdev->dev);
		class_destroy(sdev->cls);
		pr_err("simtemp: failed to create device\n");
		return ret;
	}

	/* Create sysfs attributes */
	ret = sysfs_create_group(&sdev->dev->kobj, &simtemp_attr_group);
	if (ret) {
		device_destroy(sdev->cls, 0);
		class_destroy(sdev->cls);
		pr_err("simtemp: failed to create sysfs group\n");
		return ret;
	}

	dev_info(sdev->dev, "sysfs group created under /sys/class/simtemp/\n");
	return 0;
}
EXPORT_SYMBOL_GPL(simtemp_sysfs_init);

/**
 * @brief Clean up sysfs attributes
 *
 * @param sdev
 */
void simtemp_sysfs_exit(struct simtemp_device *sdev)
{
	if (!sdev || !sdev->cls || !sdev->dev)
		return;

	sysfs_remove_group(&sdev->dev->kobj, &simtemp_attr_group);
	device_destroy(sdev->cls, 0);
	class_destroy(sdev->cls);
	dev_info(sdev->dev, "sysfs group removed\n");
}
EXPORT_SYMBOL_GPL(simtemp_sysfs_exit);
