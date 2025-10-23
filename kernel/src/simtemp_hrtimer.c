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

/*******************************************************************************
 * Variables
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/

/**
 * @brief Timer callback function to generate and push temperature samples
 *
 * @param timer
 * @return enum hrtimer_restart
 */
static enum hrtimer_restart simtemp_timer_callback(struct hrtimer *timer)
{
	struct simtemp_data *sdat = container_of(timer, struct simtemp_data, timer);
	struct simtemp_sample sample;
	s32 noise;
	u32 flags = 0;

	/* simulate small random variation: [-150, +150] m°C */
	noise = (s32)(get_random_u32() % 301) - 150;

	/* update current temperature */
	spin_lock(&sdat->lock);
	sdat->last_sample.temp_mc += noise;
	spin_unlock(&sdat->lock);

	/* fill sample */
	sample.timestamp_ns = ktime_get_ns();
	sample.temp_mc = sdat->last_sample.temp_mc;
	sample.sampling_ms = sdat->last_sample.sampling_ms;
	sample.threshold_mc = sdat->last_sample.threshold_mc;

	/* check threshold */
	if ((u32)sample.temp_mc >= sdat->last_sample.threshold_mc) {
		flags |= SIMTEMP_FLAG_THRESHOLD_CROSSED;
		wake_up_poll(&sdat->read_queue, POLLIN | POLLRDNORM | POLLPRI);
	} else {
		/* clear threshold crossed flag if condition no longer met */
		flags &= ~SIMTEMP_FLAG_THRESHOLD_CROSSED;
		wake_up_poll(&sdat->read_queue, POLLIN | POLLRDNORM);
	}
	sample.flags = flags | SIMTEMP_FLAG_NEW_SAMPLE;

	/* push into ring buffer */
	simtemp_rb_push(&sdat->rb, &sample);
	/* notifies to workqueue*/
	queue_work(sdat->wq, &sdat->work); 
	/* forward the timer and restart */
	hrtimer_forward_now(&sdat->timer, sdat->last_sample.sampling_ms);

	return HRTIMER_RESTART;
}

/**
 * @brief Initialize hrtimer
 *
 * @param sdat
 * @param sampling_ms
 * @return int
 */
int simtemp_hrtimer_init(struct simtemp_data *sdat, u32 sampling_ms)
{
	sdat->last_sample.sampling_ms = ktime_set(0, sampling_ms * 1000000LL);
	hrtimer_init(&sdat->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	sdat->timer.function = simtemp_timer_callback;
	hrtimer_start(&sdat->timer, ms_to_ktime(sdat->last_sample.sampling_ms), HRTIMER_MODE_REL);

	return 0;
}
EXPORT_SYMBOL_GPL(simtemp_hrtimer_init);

/**
 * @brief Stop and clean up hrtimer
 *
 * @param sdat
 */
void simtemp_hrtimer_exit(struct simtemp_data *sdat)
{
	hrtimer_cancel(&sdat->timer);
}
EXPORT_SYMBOL_GPL(simtemp_hrtimer_exit);
