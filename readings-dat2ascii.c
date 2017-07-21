/*
 * Copyright 2017, Low Power Company, Inc.
 * Copyright 2017, Andrew Sharp
 *
 * Program to convert power meter binary readings store to ascii
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include "extech.h"

struct timespec startclk;

/*
 * function to FUNCTIONALLY subtract a monotonic timespec value from
 * a realtime clock timespec value.  using the first monotonic value
 * it's called with as a base, and all the rest considered as a delta
 * from that one.
 */
 void
correct_tstamp(struct timespec stmp, struct timeval *r)
{
	static struct timespec s1;
	static unsigned long wrap;

	stmp.tv_nsec = (stmp.tv_nsec / 4000000) * 4000; /* accur. is 4ms */
	r->tv_sec = startclk.tv_sec;
	r->tv_usec = (startclk.tv_nsec / 4000000) * 4000;

	/*
	 * everything is in msecs from here on
	 */
	if (s1.tv_sec == s1.tv_nsec == 0L) {
		s1 = stmp;
		wrap = 1000000 - s1.tv_nsec;
	} else {
		r->tv_sec = r->tv_sec + (stmp.tv_sec - s1.tv_sec);
		if (stmp.tv_nsec < s1.tv_nsec) {
			r->tv_usec = r->tv_usec + (wrap + stmp.tv_nsec);
			r->tv_sec--;
		} else {
			r->tv_usec = r->tv_usec + (stmp.tv_nsec - s1.tv_nsec);
		}
		if (r->tv_usec > 1000000) {
			r->tv_usec -= 1000000;
			r->tv_sec++;
		}
	}
}

/*
 * argument specification
 */
int raw = 0; /* means output raw timestamp rather than ascii date/time */
int processed = 1; /* means output ascii date/time timestamp */
int maxv = 0; /* process the input file; only output the max's of each field */

struct option da_opts[] = {
	{
		"raw",
		no_argument,
		&raw,
		1
	},
	{
		"processed",
		no_argument,
		&processed,
		1
	},
	{
		"max",
		no_argument,
		&maxv,
		1
	},
	{}
};

 void
usage(char **args)
{
	struct option *o;

	printf("%s: invalid argument or command line option\n", args[0]);
	printf("The valid options are: ");
	for (o = &da_opts[0]; o->name; o++) {
		printf("--%s ", o->name);
	}
	printf("\n");
}


 int
main(int argc, char **argv) {
	int rc;
	int storefile;
	struct reading reading;
	struct reading m;
	struct timeval tvstamp;
	char tst[128];

	do {
		rc = getopt_long(argc, argv, "", &da_opts[0], NULL);
		if ((rc == ':') || (rc == '?')) {
				usage(argv);
				exit(1);
		}
		if (rc == -1) {
			break;
		}
	} while (1);

	if ((argv[optind] == NULL) || (strlen(argv[optind]) == 0)) {
		printf("first argument must be name of file to read\n");
		exit(1);
	}

	if (raw) {
		processed = 0;
	}

	/*
	 * save the readings in a binary file
	 */
	storefile = open(argv[optind], O_RDONLY);
	if (storefile < 0 ) {
		fprintf(stderr, "open storefile failed.  open returned '%d' errno=%d\n",
		storefile, errno);
	} else {
		rc = read(storefile, &startclk, sizeof(struct timespec));
		if (rc != sizeof(struct timespec)) {
			printf("failed to read start clock value from storefile - %d\n",
				rc);
		} else {
			if (!maxv) {
				if (raw) {
					printf("     timestamp   watts      pf   volts    amps\n");
				}
				if (processed) {
					printf("                   timestamp   watts      pf   volts    amps\n");
				}
			}
			while ((rc = read(storefile, &reading, sizeof(struct reading))) ==
				sizeof(struct reading)) {

				correct_tstamp((struct timespec)reading.tstamp, &tvstamp);
				if (raw & (!maxv)) {
					printf("%ld.%.3ld ",
						tvstamp.tv_sec, tvstamp.tv_usec / 1000);
				}
				if (processed & (!maxv)) {
					asctime_r(localtime((time_t *)&tvstamp.tv_sec), tst);
					sprintf(&tst[strlen(tst) - 6], ".%.3ld 2017",
						tvstamp.tv_usec / 1000);
					printf("%s ", tst);
				}
				if (!maxv) {
					printf("%7.3f %7.3f %7.3f %7.3f\n", reading.watts,
						reading.pf, reading.volts, reading.amps);
				} else {
					if (reading.watts > m.watts) {
						m.watts = reading.watts;
					}
					if (reading.pf > m.pf) {
						m.pf = reading.pf;
					}
					if (reading.volts > m.volts) {
						m.volts = reading.volts;
					}
					if (reading.amps > m.amps) {
						m.amps = reading.amps;
					}
				}
			}

			if (maxv) {
				printf("  watts      pf   volts    amps\n");
				printf("%7.3f %7.3f %7.3f %7.3f\n", m.watts, m.pf, m.volts,
					m.amps);
			}
		}
		close(storefile);
	}
}

