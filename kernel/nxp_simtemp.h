/*SPDX-License-Identifier: GPL-2.0-only*/ 
/**
 * @file nxp_simtemp.h
 * @author Luis Hernández <luishg0111@gmail.com>
 * @brief Virtual simulated temperature sensor driver
 * @version 0.1
 * @date 2025-10-15
 * 
 * Copyright (C) 2025 Luis Hernández <luishg0111@gmail.com>
 * 
 */
#ifndef _NXP_SIMTEMP_H_
#define _NXP_SIMTEMP_H_

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include <linux/types.h>

/*******************************************************************************
 * Definitions
 ******************************************************************************/
/* Default config values */
#define NXP_SIMTEMP_DEFAULT_SAMPLING_MS 100
#define NXP_SIMTEMP_DEFAULT_THRESHOLD_MILLIC 45000

/* Event flag bits */
#define SIMTEMP_FLAG_NEW_SAMPLE        (1U << 0)
#define SIMTEMP_FLAG_THRESHOLD_CROSSED (1U << 1)

/*******************************************************************************
 * Types
 ******************************************************************************/
/* Structure shared with user space */
struct simtemp_sample{
    __u64 timestamp_ns;  /* timestamp */
    __s32 temp_mC;      /* milli-degree celsius */
    __u32 flags;        /* status/event flags */
} __attribute__((packed));

#endif /* _NXP_SIMTEMP_H_ */
