/*
 * Copyright 2017, Low Power Company, Inc.
 * Copyright 2017, Andrew Sharp
 *
 * Program to read the measured values from an extech power meter
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include "extech.h"

#define RS_NELEMENTS 9000

/*
 * pointer is used here in case we ever want to
 * dynamically allocate more arrays in addition to the static one.
 * or just do it all dynamically.
 */
struct reading *rsp; 

struct reading readings_store[RS_NELEMENTS];

int so_rs;
extern int rs;  /* write index into the readings_store array */

struct timespec startclk;


 int
main(int argc, char **argv) {
	int rc;
	int storefile;

	/*
	 * initialize rsp for use by linked functions
	 */
	rsp = &readings_store[0];
	so_rs = RS_NELEMENTS;

	// printf("size of readings_store: %ld\n", sizeof(readings_store));
	// for RS_NELEMENTS=9000, this is 288000, or 281.25 KiB

	rc = extech_power_meter("/dev/ttyUSB0");
	if (rc) {
		dprintf(2, "extech_power_meter function returned error code '%d'\n", rc);
	}
	printf("starting measurement process and sleeping for 10s...\n");
	start_measurement();
	sleep(10);
	end_measurement();

	printf("joules consumed: %g\n", ex_joules_consumed());

	/*
	 * save the readings in a binary file
	 */
	storefile = open("readings.dat.00", O_CREAT | O_EXCL | O_RDWR, 0644);
	if (storefile < 0 ) {
		fprintf(stderr, "open storefile failed.  open returned '%d' errno=%d\n",
		storefile, errno);
	} else {
		rc = write(storefile, &startclk, sizeof(struct timespec));
		rc = write(storefile, &readings_store[0], sizeof(struct reading) * rs);
		close(storefile);
		printf("saved %d readings to readings.dat.00\n",
			rc / (int)sizeof(struct reading));
	}
}

