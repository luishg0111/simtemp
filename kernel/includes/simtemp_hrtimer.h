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
int simtemp_hrtimer_init(struct simtemp_data *sdat, u32 sampling_ms);
void simtemp_hrtimer_exit(struct simtemp_data *sdat);

/* Helpers  */
bool simtemp_ring_has_data(struct simtemp_data *sdat);
int  simtemp_ring_pop(struct simtemp_data *sdat,
		      struct simtemp_sample *out);
void simtemp_ring_push(struct simtemp_data *sdat,
		       const struct simtemp_sample *sample);
 /*******************************************************************************
 * Variables
 ******************************************************************************/

 #endif /* _SIMTEMP_HRTIMER_H_ */