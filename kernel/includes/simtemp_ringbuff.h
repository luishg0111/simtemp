// SPDX-License-Identifier: GPL-2.0-only
/**
 * @file simtemp_ringbuff.h
 * @author Luis Hernández <luishg0111@gmail.com>
 * @brief
 * @version 1.0
 * @date 2025-10-15
 *
 * @copyright Copyright (C) 2025 Luis Hernández <luishg0111@gmail.com>
 *
 */
#ifndef _SIMTEMP_RINGBUFF_H_
#define _SIMTEMP_RINGBUFF_H_

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "simtemp_core.h"
 /*******************************************************************************
 * Definitions
 ******************************************************************************/

 /*******************************************************************************
 * Types
 ******************************************************************************/

 /*******************************************************************************
 * Prototypes
 ******************************************************************************/
bool simtemp_rb_has_data(struct ring_buffer *rb);
int  simtemp_rb_pop(struct ring_buffer *rb, struct simtemp_sample *out);
void simtemp_rb_push(struct ring_buffer *rb, const struct simtemp_sample *sample);
 /*******************************************************************************
 * Variables
 ******************************************************************************/


#endif /* _SIMTEMP_RINGBUFF_H_ */