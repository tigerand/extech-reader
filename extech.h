/*
 * Copyright 2017-2019, Low Power Company, Inc.
 * Copyright 2017-2019, Andrew Sharp
 *
 * Copyright 2010, Intel Corporation
 *
 * The original version of this file was part of PowerTOP
 *
 * This program file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program in a file named COPYING; if not, write to the
 * Free Software Foundation, Inc,
 * 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 * or just google for it.
 *
 * Authors:
 *	Arjan van de Ven <arjan@linux.intel.com>
 */
#ifndef _EXTECH_H
#define _EXTECH_H

#include <pthread.h>
#include "measurement.h"


#define DEBUG
#undef DEBUG
#define EXTECH_DEBUG_PROTO 1
#undef EXTECH_DEBUG_PROTO

#ifdef DEBUG
# define debugp(A, B...) fprintf(stderr, A "\n", B)
#else
# define debugp(A, B...)
#endif

/*
 * complete numbskull hack, and won't catch negative integers and other
 * stuff masquerading as pointers
 */
#define ISPOINTER(A) ((unsigned long long)(A) > 0x1000ULL)

extern int extech_power_meter(const char *_dev_name);
extern double ex_joules_consumed(void);
extern void start_measurement(void);
extern void end_measurement(void);

struct power_meter {
	char dev_name[128];
	int fd;

	double rate;
	double sum;
	int samples;
	int end_thread;
	pthread_t thread;
};

struct reading {
	struct timespec tstamp;
	float watts;
	float pf;
	float volts;
	float amps;
};

#endif
