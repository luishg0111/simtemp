/*SPDX-License-Identifier: GPL-2.0-only*/ 
/**
 * @file simtemp_core.h
 * @author Luis Hernández <luishg0111@gmail.com>
 * @brief Virtual simulated temperature sensor driver
 * @version 0.1
 * @date 2025-10-15
 * 
 * @copyright Copyright (C) 2025 Luis Hernández <luishg0111@gmail.com>
 * 
 */
#ifndef _SIMTEMP_CORE_H_
#define _SIMTEMP_CORE_H_

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/hrtimer.h>
#include <linux/wait.h>
#include <linux/device.h>
#include <linux/miscdevice.h>

/*******************************************************************************
 * Definitions
 ******************************************************************************/
/* Default config values */
#define SIMTEMP_DEFAULT_SAMPLING_MS 100
#define SIMTEMP_DEFAULT_THRESHOLD_MILLIC 45000

/* Event flag bits */
#define SIMTEMP_FLAG_NEW_SAMPLE        (1U << 0)
#define SIMTEMP_FLAG_THRESHOLD_CROSSED (1U << 1)

#define DRIVER_NAME "nxp_simtemp"
#define DEVICE_NAME "simtemp"
#define COMPATIBLE_NAME "nxp,simtemp"

#define RING_BUFF_SIZE 128
/*******************************************************************************
 * Types
 ******************************************************************************/
/* Structure shared with user space */
struct simtemp_sample{
    __u64 timestamp_ns;     /* timestamp */
    __s32 temp_mC;          /* milli-degree celsius */
    __u32 flags;            /* status/event flags */
} __attribute__((packed));

/* Internal device data */
struct simtemp_data {	
	struct hrtimer timer;   /* timer */
	ktime_t period;         /* period */
  	
	struct simtemp_sample samples[RING_BUFF_SIZE];  /* ring buffer of samples */
	unsigned int head;      /* next write position */
	unsigned int tail;      /* next read position */
	unsigned int count;     /* number of samples present */
    wait_queue_head_t wq;   /* waitqueue for readers */

    int temp_mC;            /* current temperature in milli-degrees C */
    int threshold_mC;       /* alert threshold (milli-deg C) */
    int sampling_ms;        /* sampling interval (ms) */
    u64 total_samples;
	
    struct simtemp_sample last_sample; /* last sample read */
	spinlock_t lock;        /* sync */

    struct miscdevice miscdev;  /* misc device for /dev/simtemp */

	struct device *dev;     /* device for dev_info */
    
    struct device *sysfs_dev;  /* sysfs device */

};

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/


#endif /* _SIMTEMP_CORE_H_ */
