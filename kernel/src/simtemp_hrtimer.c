// SPDX-License-Identifier: GPL-2.0-only
/**
 * @file simtemp_hrtimer.c
 * @author Luis Hernández <luishg0111@gmail.com>
 * @brief Implementation of high-resolution timer for simulated temperature sensor
 * @version 0.1
 * @date 2025-10-15
 *
 * @copyright Copyright (C) 2025 Luis Hernández <luishg0111@gmail.com>
 *
 */
/*******************************************************************************
 * Includes
 ******************************************************************************/
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/spinlock.h>
#include <linux/random.h>
#include <linux/poll.h>

#include "simtemp_hrtimer.h"

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
static enum hrtimer_restart simtemp_timer_callback(struct hrtimer *t);

static void temp_sample_behavior(enum simtemp_mode mode, s32 *temp);
/*******************************************************************************
 * Variables
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * @brief Apply mode-specific behavior to temperature sample
 *
 * @param mode
 * @param temp
 */
static void temp_sample_behavior(enum simtemp_mode mode, s32 *temp)
{
	s32 noise;
	/* mode-specific behavior */
	switch (mode) {
	case NORMAL:
		/* no additional behavior */
		break;
	case NOISY:
		/* larger random variation: [-100, +100] m°C */
		noise = (s32)(get_random_u32() % 2001) - 1000;
		*temp += noise;
		break;
	case RAMP:
		/* increase temperature by 100 m°C per sample */
		*temp += 100;
		if (*temp > SIMTEMP_TEMPERATURE_MC_MAX)
			*temp = SIMTEMP_TEMPERATURE_MC_MIN; /* wrap around */
		break;
	default:
		/* unknown mode, log error and use NORMAL behavior */
		break;
	}
}

/**
 * @brief Timer callback function to generate and push temperature samples
 *
 * @param timer
 * @return enum hrtimer_restart
 */
static enum hrtimer_restart simtemp_timer_callback(struct hrtimer *timer)
{
	struct simtemp_device *sdev = container_of(timer, struct simtemp_device, timer);
	struct simtemp_sample sample;
	enum simtemp_mode mode;
	s32 threshold, temp;
	unsigned long devflags;
	u32 sflags = SIMTEMP_FLAG_NEW_SAMPLE;

	/* Access configuration under lock */
	temp = sdev->last_sample.temp_mc;
	threshold = sdev->threshold_mc;
	mode = sdev->mode;

	temp_sample_behavior(mode, &temp);

	/* update current temperature */
	sample.timestamp_ns = ktime_get_ns();
	sample.temp_mc      = temp;
	sample.flags        = sflags;

	/* Update global state and stats */
	spin_lock_irqsave(&sdev->device_lock, devflags);
	sdev->last_sample = sample;
	sdev->stats.updates_count++;

	/* check threshold */
	if (temp >= sdev->threshold_mc) {
		sdev->stats.alerts_count++;
		sflags |= SIMTEMP_FLAG_THRESHOLD_CROSSED;
		sdev->stats.alert_pending = true;
		wake_up_poll(&sdev->read_queue, POLLIN | POLLRDNORM | POLLPRI);
	} else {
		/* clear threshold crossed flag if condition no longer met */
		sflags &= ~SIMTEMP_FLAG_THRESHOLD_CROSSED;
		wake_up_poll(&sdev->read_queue, POLLIN | POLLRDNORM);
	}
	sample.flags = sflags;
	spin_unlock_irqrestore(&sdev->device_lock, devflags);

	/* push into ring buffer */
	simtemp_rb_push(&sdev->rb, &sample);
	/* wake-up block events*/
	wake_up_interruptible(&sdev->read_queue);
	/* forward the timer and restart */
	hrtimer_forward_now(&sdev->timer, ms_to_ktime(READ_ONCE(sdev->sampling_ms)));

	return HRTIMER_RESTART;
}

/**
 * @brief Initialize hrtimer
 *
 * @param sdev
 * @param sampling_ms
 * @return int
 */
int simtemp_hrtimer_init(struct simtemp_device *sdev, u32 sampling_ms)
{
	/* Init ringbuff */
	sdev->rb.head = 0;
	sdev->rb.tail = 0;

	hrtimer_init(&sdev->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	sdev->timer.function = simtemp_timer_callback;
	hrtimer_start(&sdev->timer, ms_to_ktime(READ_ONCE(sdev->sampling_ms)), HRTIMER_MODE_REL);

	pr_info("%s: hrtimer started (%u ms)\n", DRIVER_NAME, sdev->sampling_ms);

	return 0;
}
EXPORT_SYMBOL_GPL(simtemp_hrtimer_init);

/**
 * @brief Stop and clean up hrtimer
 *
 * @param sdev
 */
void simtemp_hrtimer_exit(struct simtemp_device *sdev)
{
	hrtimer_cancel(&sdev->timer);
}
EXPORT_SYMBOL_GPL(simtemp_hrtimer_exit);
