/*SPDX-License-Identifier: GPL-2.0-only*/ 
/**
 * @file simtemp_hrtimer.h
 * @author Luis Hernández <luishg0111@gmail.com>
 * @brief 
 * @version 0.1
 * @date 2025-10-15
 * 
 * @copyright Copyright (C) 2025 Luis Hernández <luishg0111@gmail.com>
 * 
 */
#ifndef _SIMTEMP_HRTIMER_H_
#define _SIMTEMP_HRTIMER_H_

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include <linux/hrtimer.h>

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
/* API exposed by hrtimer engine */
int simtemp_hrtimer_init(struct simtemp_data *sdev, u32 sampling_ms);
void simtemp_hrtimer_exit(struct simtemp_data *sdev);

/* push new sample manually (for testing) */
enum hrtimer_restart simtemp_timer_callback(struct hrtimer *t);
void ring_push_sample(struct simtemp_data *s, const struct simtemp_sample *sample);
int ring_pop_sample(struct simtemp_data *s, struct simtemp_sample *out);
 /*******************************************************************************
 * Variables
 ******************************************************************************/

 #endif /* _SIMTEMP_HRTIMER_H_ */