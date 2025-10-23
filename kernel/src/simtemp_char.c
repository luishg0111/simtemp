// SPDX-License-Identifier: GPL-2.0-only
/**
 * @file simtemp_char.c
 * @author Luis Hernández <luishg0111@gmail.com>
 * @brief Character device interface for simulated temperature sensor
 * @version 0.1
 * @date 2025-10-15
 *
 * @copyright Copyright (C) 2025 Luis Hernández <luishg0111@gmail.com>
 *
 * Author: Luis Hernández <luishg0111@gmail.com>
 * Date:   2025-10-15
 */
/*******************************************************************************
 * Includes
 ******************************************************************************/
#include <linux/poll.h>
#include <linux/spinlock.h>

#include "simtemp_char.h"

#include "simtemp_core.h"
#include "simtemp_ringbuff.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Types
 ******************************************************************************/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static int simtemp_open(struct inode *inode, struct file *file);
static ssize_t simtemp_read(struct file *file, char __user *buf, size_t len, loff_t *ppos);
static __poll_t simtemp_poll(struct file *file, poll_table *wait);
/*******************************************************************************
 * Variables
 ******************************************************************************/
static const struct file_operations simtemp_fops = {
	.owner	= THIS_MODULE,
	.open	= simtemp_open,
	.read	= simtemp_read,
	.poll	= simtemp_poll,
};

/*******************************************************************************
 * Code
 ******************************************************************************/

/**
 *@brief Open function for simtemp character device
 *
 *@param inode
 *@param file
 *@return int
 */
static int simtemp_open(struct inode *inode, struct file *file)
{
	struct miscdevice *mdev = file->private_data;
	struct simtemp_data *sdat = container_of(mdev, struct simtemp_data, miscdev);

	file->private_data = sdat;

	return 0;
}

/**
 * @brief Read function for simtemp character device
 *
 * @param file
 * @param buf
 * @param len
 * @param ppos
 * @return ssize_t
 */
static ssize_t simtemp_read(struct file *file, char __user *buf, size_t len, loff_t *ppos)
{
	struct simtemp_data *sdat = file->private_data;
	unsigned long flags;
	int ret;

	if (len < sizeof(sdat->last_sample)) {
		pr_warn("%s: Read requested size %zu is too small. Expected %zu.\n",
			DRIVER_NAME, len, sizeof(sdat->last_sample));
		return -EINVAL;
	}

	/* wait for data unless O_NONBLOCK */
	if(!simtemp_rb_has_data(&sdat->rb)) {
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		if (wait_event_interruptible(sdat->read_queue, simtemp_rb_has_data(&sdat->rb)))
			return -ERESTARTSYS;
	}
	ret = simtemp_rb_pop(&sdat->rb, &sdat->last_sample);
	if (ret)
		return 0;

	spin_lock_irqsave(&sdat->lock, flags);
	if (copy_to_user(buf, &sdat->last_sample, sizeof(sdat->last_sample))) {
		spin_unlock_irqrestore(&sdat->lock, flags);
		return -EFAULT;
	}
	ret = sizeof(sdat->last_sample);

	/* Clear the NEW_SAMPLE flag for the next read/wait cycle */
	sdat->last_sample.flags &= ~SIMTEMP_FLAG_NEW_SAMPLE;
	spin_unlock_irqrestore(&sdat->lock, flags);

	return ret;
}

/**
 * @brief Poll function for simtemp character device
 *
 * @param file
 * @param wait
 * @return __poll_t
 */
static __poll_t simtemp_poll(struct file *file, poll_table *wait)
{
	struct simtemp_data *sdat = file->private_data;
	unsigned long flags;
	__poll_t mask = 0;

	poll_wait(file, &sdat->read_queue, wait);

	spin_lock_irqsave(&sdat->lock, flags);
	if (simtemp_rb_has_data(&sdat->rb))
		mask |= POLLIN | POLLRDNORM;

	if (sdat->last_sample.flags & SIMTEMP_FLAG_THRESHOLD_CROSSED)
		mask |= POLLPRI;
	spin_unlock_irqrestore(&sdat->lock, flags);

	return mask;
}

/**
 * @brief Initialize the simtemp character device
 *
 * @param sdat
 * @return int
 */
int simtemp_char_init(struct simtemp_data *sdat)
{
	int ret;

	sdat->miscdev.minor = MISC_DYNAMIC_MINOR;
	sdat->miscdev.name  = DEVICE_NAME;
	sdat->miscdev.fops  = &simtemp_fops;

	ret = misc_register(&sdat->miscdev);
	if (ret) {
		dev_err(sdat->dev, "failed to register misc device: %d\n", ret);
		return ret;
	}

	dev_info(sdat->dev, "simtemp char device ready: /dev/%s\n", sdat->miscdev.name);

	return ret;
}
EXPORT_SYMBOL_GPL(simtemp_char_init);

/**
 * @brief Clean up the simtemp character device
 *
 * @param sdat
 */
void simtemp_char_exit(struct simtemp_data *sdat)
{
	misc_deregister(&sdat->miscdev);
}
EXPORT_SYMBOL_GPL(simtemp_char_exit);
