/*
 * Copyright 2017, Low Power Company, Inc.
 * Copyright 2017, Andrew Sharp
 *
 * Copyright 2010, Intel Corporation
 *
 * This file was part of PowerTOP
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
//#include "sysfs.h"
//#include "../parameters/parameters.h"
//#include "../lib.h"

#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <unistd.h>
#include "measurement.h"
#include "extech.h"

extern double ex_joules_consumed(void);


/*
 *  void
 * start_measurement(void)
 * {
 * }
 * 
 * 
 *  void
 * end_measurement(void)
 * {
 * }
 */

 double
joules_consumed(void)
{
	double total = 0.0;
	unsigned int i;

	total += ex_joules_consumed();

	return total;
}

/*
 * could prove slightly useful in a later, completely different, context
 *
 void
detect_power_meters(void)
{
	process_directory("/sys/class/power_supply", sysfs_power_meters_callback);
	if (power_meters.size() == 0) {
		process_directory("/proc/acpi/battery", acpi_power_meters_callback);
	}
}
 */
