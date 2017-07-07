/*
 * Copyright 2017, Low Power Company, Inc.
 * Copyright 2017, Andrew Sharp
 *
 * Program to read the measured values from an extech power meter
 */

#include <stdio.h>
#include <fcntl.h>
#include "extech.h"

main(int argc, char **argv) {
	int rc;

	rc = extech_power_meter("/dev/ttyUSBS0");
	if (rc) {
		dprintf(2, "extech_power_meter function returned error code '%d'\n", rc);
	}
}
