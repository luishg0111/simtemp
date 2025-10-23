// SPDX-License-Identifier: GPL-2.0-only
/**
 * @file simtemp_ringbuff.c
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
#include <linux/ktime.h>
#include <linux/spinlock.h>
#include <linux/random.h>

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
/* Inline functions */
static inline unsigned int ring_next(unsigned int i);

/*******************************************************************************
 * Variables
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * @brief Advance the ring buffer index
 *
 * @param i
 * @return unsigned int
 */
static inline unsigned int ring_next(unsigned int i)
{
	return (i + 1) % RING_BUFF_SIZE;
}

/**
 * @brief Check if ring buffer has data
 * 
 * @param rb 
 * @return true 
 * @return false 
 */
bool simtemp_rb_has_data(struct ring_buffer *rb)
{
	/* lockless read; safe enough for wait condition */
	return rb->head != rb->tail;
}
EXPORT_SYMBOL_GPL(simtemp_rb_has_data);

/**
 * @brief Push a new sample into the ring buffer
 *
 * @param rb
 * @param sample
 */
void simtemp_rb_push(struct ring_buffer *rb,
		       const struct simtemp_sample *sample)
{
	unsigned long flags;

	spin_lock_irqsave(&rb->lock, flags);
	rb->samples[rb->head] = *sample;
	rb->head = ring_next(rb->head);
	if (rb->head == rb->tail)
		rb->tail = ring_next(rb->tail); /* buffer full: advance tail */
	spin_unlock_irqrestore(&rb->lock, flags);
}
EXPORT_SYMBOL_GPL(simtemp_rb_push);

/**
 * @brief Pop a sample from the ring buffer
 *
 * @param rb
 * @param out
 * @return int
 */
int simtemp_rb_pop(struct ring_buffer *rb, struct simtemp_sample *out)
{
	unsigned long flags;
	int empty;

	spin_lock_irqsave(&rb->lock, flags);
	empty = (rb->head == rb->tail);
	if (!empty) {
		*out = rb->samples[rb->tail];
		rb->tail = ring_next(rb->tail);
	}
	spin_unlock_irqrestore(&rb->lock, flags);

	return empty ? -1 : 0;
}
EXPORT_SYMBOL_GPL(simtemp_rb_pop);
