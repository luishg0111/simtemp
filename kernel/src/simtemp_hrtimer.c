/*SPDX-License-Identifier: GPL-2.0-only*/ 
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

#include "simtemp_hrtimer.h"
 /*******************************************************************************
 * Definitions
 ******************************************************************************/

 /*******************************************************************************
 * Types
 ******************************************************************************/

 /*******************************************************************************
 * Prototypes
 ******************************************************************************/
static inline unsigned int ring_next(unsigned int i);

 /*******************************************************************************
 * Variables
 ******************************************************************************/

 /*******************************************************************************
 * Code
 ******************************************************************************/
 /* Helper: advance index modulo RING_BUFF_SIZE */
static inline unsigned int ring_next(unsigned int i)
{
	return (i + 1) % RING_BUFF_SIZE;
}

bool simtemp_ring_has_data(struct simtemp_data *sdat)
{
	/* lockless read; safe enough for wait condition */
	return READ_ONCE(sdat->count) > 0;
}

/* Push a sample into ring buffer (overwrites oldest if full) */
void simtemp_ring_push(struct simtemp_data *sdat, const struct simtemp_sample *sample)
{
	unsigned long flags;
	spin_lock_irqsave(&sdat->lock, flags);

	sdat->samples[sdat->head] = *sample;
	sdat->head = ring_next(sdat->head);
	if (sdat->count < RING_BUFF_SIZE) {
		sdat->count++;
	} else {
		/* buffer full: advance tail (overwrite oldest) */
		sdat->tail = ring_next(sdat->tail);
	}
	spin_unlock_irqrestore(&sdat->lock, flags);
}

/* Pop one sample from ring buffer. Returns 0 on success, -ENODATA if empty */
int simtemp_ring_pop(struct simtemp_data *sdat, struct simtemp_sample *out)
{
	unsigned long flags;
	int ret = -ENODATA;

	spin_lock_irqsave(&sdat->lock, flags);
	if (sdat->count == 0) {
		ret = -ENODATA;
	} else {
		*out = sdat->samples[sdat->tail];
		sdat->tail = ring_next(sdat->tail);
		sdat->count--;
		ret = 0;
	}
	spin_unlock_irqrestore(&sdat->lock, flags);
	return ret;
}

/* Timer callback: simulate a new temperature sample and push to ring */
enum hrtimer_restart simtemp_timer_callback(struct hrtimer *timer)
{
	struct simtemp_data *sdat = container_of(timer, struct simtemp_data, timer);
	struct simtemp_sample sample;
	s32 noise;
	u64 count;
	u32 flags = 0;

	/* simulate small random variation: [-150, +150] m°C */
	noise = (s32)(get_random_u32() % 301) - 150;

	/* update current temperature */
	spin_lock(&sdat->lock);
	sdat->temp_mC += noise;
	sdat->total_samples++;
	spin_unlock(&sdat->lock);

	count = ++sdat->total_samples;
	/*Debug output every 10 samples */
	if (count % 10 == 0)
        dev_info(sdat->dev, "[simtemp] sample #%llu: %d mC\n",
                 (unsigned long long)count,
                 sdat->temp_mC);

	/* fill sample */
	sample.timestamp_ns = ktime_get_ns();
	sample.temp_mC = sdat->temp_mC;

	/* check threshold */
	if ((u32)sample.temp_mC >= sdat->threshold_mC)
		flags |= SIMTEMP_FLAG_THRESHOLD_CROSSED;

	sample.flags = flags | SIMTEMP_FLAG_NEW_SAMPLE;

	/* push into ring buffer */
	simtemp_ring_push(sdat, &sample);

	/* NOTE: later we will wake waitqueues to notify readers (when poll implemented) */

	/* forward the timer and restart */
	hrtimer_forward_now(&sdat->timer, sdat->period);
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
	sdat->period = ktime_set(0, sampling_ms * 1000000LL);
	hrtimer_init(&sdat->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	sdat->timer.function = simtemp_timer_callback;
	hrtimer_start(&sdat->timer, sdat->period, HRTIMER_MODE_REL);
	return 0;
}

/**
 * @brief Stop and clean up hrtimer
 * 
 * @param sdat 
 */
void simtemp_hrtimer_exit(struct simtemp_data *sdat)
{
	hrtimer_cancel(&sdat->timer);
}

EXPORT_SYMBOL_GPL(simtemp_hrtimer_init);
EXPORT_SYMBOL_GPL(simtemp_hrtimer_exit);
EXPORT_SYMBOL_GPL(simtemp_ring_has_data);