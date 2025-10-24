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
/** Logging configuration (DEBUG vs RELEASE) */
#ifdef DEBUG
    /* DEBUG activates pr_debug() y dev_dbg() */
    #define simtemp_dbg(dev, fmt, ...)  dev_dbg(dev, "[DBG] " fmt, ##__VA_ARGS__)
    #define simtemp_pr_dbg(fmt, ...)    pr_debug("[DBG] " fmt, ##__VA_ARGS__)
#else
    /* RELEASE mode removes all the driver prints */
    #define simtemp_dbg(dev, fmt, ...)  do { } while (0)
    #define simtemp_pr_dbg(fmt, ...)    do { } while (0)
#endif

/* Driver names */
#define DRIVER_NAME "nxp_simtemp"
#define DEVICE_NAME "simtemp"
#define CLASS_NAME  "simtemp"
#define COMPATIBLE_NAME "nxp,simtemp"

/* Default config values */
#define SIMTEMP_DEFAULT_SAMPLING_MS 1000
#define SIMTEMP_DEFAULT_THRESHOLD_MILLIC 45000
#define SIMTEMP_DEFAULT_TEMPERATURE_MC 36000
#define SIMTEMP_DEFAULT_TIMESTAMP_NS 0ULL
#define SIMTEMP_DEFAULT_FLAGS 0 /* NORMAL mode */
#define SIMTEMP_DEFAULT_MODE 0 /* NORMAL mode */

/* Global limits for period sampling_ms*/
#define SIMTEMP_SAMPLING_MS_MAX 10000u
#define SIMTEMP_SAMPLING_MS_MIN 1u

/* Global limits for period sampling_ms*/
#define SIMTEMP_TEMPERATURE_MC_MAX 10000
#define SIMTEMP_TEMPERATURE_MC_MIN -5000

/* Event flag bits */
#define SIMTEMP_FLAG_NEW_SAMPLE        (1U << 0)
#define SIMTEMP_FLAG_THRESHOLD_CROSSED (1U << 1)

/* Ring buffer size */
#define RING_BUFF_SIZE 512
/*******************************************************************************
 * Types
 ******************************************************************************/
/* Structure shared with user space */
struct simtemp_sample{
	__u64 timestamp_ns;	/* timestamp */
	__s32 temp_mc;		/* milli-degree celsius */
	__u32 flags;		/* status/event flags */
} __attribute__((packed));

enum simtemp_mode {
	NORMAL = 0,	/* Default mode */
	NOISY,		/* Noisy mode */
	RAMP		/* Ramp mode */
};

struct ring_buffer {
	struct simtemp_sample samples[RING_BUFF_SIZE];	/* ring buffer of samples */
	unsigned int head;   				/* next write position */
	unsigned int tail;   				/* next read position */
	spinlock_t lock;				/* lock for concurrent access */
};
struct simtemp_stats {
	unsigned long updates_count;
	unsigned long alerts_count;
	unsigned long errors_count;
	bool alert_pending;
};
/* Internal device data */
struct simtemp_device {
	struct miscdevice miscdev;		/* misc device for /dev/simtemp */
	struct device *dev;			/* device for dev_info */
	struct class *cls;	

	struct hrtimer timer;			/* timer */
	u32 sampling_ms;     			/* sampling interval (ms) */
	s32 threshold_mc;			/* alert threshold (milli-deg C) */
	struct simtemp_stats stats; 		/* statistics */
	enum simtemp_mode mode;     		/* operating mode */

	struct ring_buffer rb;			/* ring buffer */
	wait_queue_head_t read_queue;		/* wait queue for readers */
	struct simtemp_sample last_sample; 	/* last sample read */
	
	spinlock_t device_lock;			/* protects critical */
	struct mutex device_mutex;		/* protects non-critical operation */
};

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/


#endif /* _SIMTEMP_CORE_H_ */
