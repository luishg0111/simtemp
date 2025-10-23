// SPDX-License-Identifier: GPL-2.0-only
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
#include <linux/workqueue.h>

/*******************************************************************************
 * Definitions
 ******************************************************************************/
/* Driver names */
#define DRIVER_NAME "nxp_simtemp"
#define DEVICE_NAME "simtemp"
#define CLASS_NAME  "simtemp_class"
#define COMPATIBLE_NAME "nxp,simtemp"

/* Default config values */
#define SIMTEMP_DEFAULT_SAMPLING_MS 100
#define SIMTEMP_DEFAULT_THRESHOLD_MILLIC 45000
#define SIMTEMP_DEFAULT_TEMPERATURE_MC 42000
#define SIMTEMP_DEFAULT_TIMESTAMP_NS 0ULL
/* Event flag bits */
#define SIMTEMP_FLAG_NEW_SAMPLE        (1U << 0)
#define SIMTEMP_FLAG_THRESHOLD_CROSSED (1U << 1)

/* Ring buffer size */
#define RING_BUFF_SIZE 128
/*******************************************************************************
 * Types
 ******************************************************************************/
/* Structure shared with user space */
struct simtemp_sample{
	__u64 timestamp_ns;	/* timestamp */
	__u32 sampling_ms;	/* sampling interval (ms) */
	__s32 temp_mc;		/* milli-degree celsius */
	__s32 threshold_mc;	/* alert threshold (milli-deg C) */
	__u32 flags;		/* status/event flags */
} __attribute__((packed));

enum simtemp_mode {
	NORMAL,		/* Default mode */
	NOISY,		/* Noisy mode */
	RAMP		/* Ramp mode */
};

struct ring_buffer {
	struct simtemp_sample samples[RING_BUFF_SIZE];	/* ring buffer of samples */
	unsigned int head;   					/* next write position */
	unsigned int tail;   					/* next read position */
	spinlock_t lock;				/* lock for concurrent access */
};
struct simtemp_stats {
	unsigned long updates_count;
	unsigned long alerts_count;
	unsigned long errors_count;
};
/* Internal device data */
struct simtemp_data {
	struct miscdevice miscdev;		/* misc device for /dev/simtemp */
	struct device *dev;			/* device for dev_info */
	struct hrtimer timer;			/* timer */

	struct ring_buffer rb;			/* ring buffer */
	struct workqueue_struct *wq;		/* workqueue */
	struct work_struct work;		/* work struct for readers */	
	wait_queue_head_t read_queue;		/* wait queue for readers */

	struct simtemp_stats stats; 		/* statistics */
	enum simtemp_mode mode;     		/* operating mode */

	struct simtemp_sample last_sample; 	/* last sample read */
	spinlock_t lock;        /* sync */
	struct mutex avg_lock;  /* sync for average calculation */
};

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/


#endif /* _SIMTEMP_CORE_H_ */
