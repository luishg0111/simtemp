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
 /* Helper: advance index modulo RING_SIZE */
static inline unsigned int ring_next(unsigned int i)
{
	return (i + 1) % RING_SIZE;
}

/* Push a sample into ring buffer (overwrites oldest if full) */
void ring_push_sample(struct simtemp_data *s, const struct simtemp_sample *sample)
{
	unsigned long flags;
	spin_lock_irqsave(&s->lock, flags);

	s->samples[s->head] = *sample;
	s->head = ring_next(s->head);
	if (s->count < RING_SIZE) {
		s->count++;
	} else {
		/* buffer full: advance tail (overwrite oldest) */
		s->tail = ring_next(s->tail);
	}
	spin_unlock_irqrestore(&s->lock, flags);
}

/* Pop one sample from ring buffer. Returns 0 on success, -ENODATA if empty */
int ring_pop_sample(struct simtemp_data *s, struct simtemp_sample *out)
{
	unsigned long flags;
	int ret = -ENODATA;

	spin_lock_irqsave(&s->lock, flags);
	if (s->count == 0) {
		ret = -ENODATA;
	} else {
		*out = s->samples[s->tail];
		s->tail = ring_next(s->tail);
		s->count--;
		ret = 0;
	}
	spin_unlock_irqrestore(&s->lock, flags);
	return ret;
}

/* Timer callback: simulate a new temperature sample and push to ring */
enum hrtimer_restart simtemp_timer_callback(struct hrtimer *timer)
{
	struct simtemp_data *s = container_of(timer, struct simtemp_data, timer);
	struct simtemp_sample sample;
	s32 noise;
	u64 count;
	u32 flags = 0;

	/* simulate small random variation: [-150, +150] m°C */
	noise = (s32)(get_random_u32() % 301) - 150;

	/* update current temperature */
	spin_lock(&s->lock);
	s->temp_mC += noise;
	s->total_samples++;
	spin_unlock(&s->lock);

	count = ++s->total_samples;
	/*Debug output every 10 samples */
	if (count % 10 == 0)
        dev_info(s->dev, "[simtemp] sample #%llu: %d mC\n",
                 (unsigned long long)count,
                 s->temp_mC);

	/* fill sample */
	sample.timestamp_ns = ktime_get_ns();
	sample.temp_mC = s->temp_mC;

	/* check threshold */
	if ((u32)sample.temp_mC >= s->threshold_mC)
		flags |= SIMTEMP_FLAG_THRESHOLD_CROSSED;

	sample.flags = flags | SIMTEMP_FLAG_NEW_SAMPLE;

	/* push into ring buffer */
	ring_push_sample(s, &sample);

	/* NOTE: later we will wake waitqueues to notify readers (when poll implemented) */

	/* forward the timer and restart */
	hrtimer_forward_now(&s->timer, s->period);
	return HRTIMER_RESTART;
}

/**
 * @brief Initialize hrtimer 
 * 
 * @param sdev 
 * @param sampling_ms 
 * @return int 
 */
int simtemp_hrtimer_init(struct simtemp_data *sdev, u32 sampling_ms)
{
	sdev->period = ktime_set(0, sampling_ms * 1000000LL);
	hrtimer_init(&sdev->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	sdev->timer.function = simtemp_timer_callback;
	hrtimer_start(&sdev->timer, sdev->period, HRTIMER_MODE_REL);
	return 0;
}

/**
 * @brief Stop and clean up hrtimer
 * 
 * @param sdev 
 */
void simtemp_hrtimer_exit(struct simtemp_data *sdev)
{
	hrtimer_cancel(&sdev->timer);
}

EXPORT_SYMBOL_GPL(simtemp_hrtimer_init);
EXPORT_SYMBOL_GPL(simtemp_hrtimer_exit);