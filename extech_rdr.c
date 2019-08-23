/*
 * Copyright 2017-2019, Low Power Company, Inc.
 * Copyright 2017-2019, Andrew Sharp
 *
 * Program to read the measured values from an extech power meter
 *
 * A run is made collecting power readings from the meter for the
 * specified period in seconds.  The total amount of power consumed in
 * watts over the course of the run is then output to stdout.
 *
 * There's two additional modes of operation: using the max option
 * will output to stdout the max reading during the run; using the
 * store-file option will store the readings taken during the run to
 * a file.  the stored readings can then be post-processed by various
 * helper programs, or you can write your own.
 *
 * A time value for how many seconds the program will run must always be
 * specified on the command line.  Using zero as the number of seconds to
 * run will cause the program to run for the maximum, which at the time of
 * this writing is 3600 seconds (1 hour).  This is changeable by changing
 * the MAX_MPERIOD define.  But it will also allow you to send SIGUSR1
 * to the program which will cause it to stop taking readings at that
 * time, and compute the amount of watts consumed accordingly.
 *
 * the MAX values possibly are not the same max that you would
 * get from the MAX button on the extech power meter.  see comments in
 * that code for more information.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "extech.h"

#define MAX_MPERIOD 3600 /* maximum number of seconds for a run */

/*
 * argument specification
 */
char **argvec;

int storefile_opt = 0; /* means store readings to a file */
int maxv = 0; /* process the input file; only output the max's of each field */
int helpout = 0; /* output basic help text */

struct option er_opts[] = {
	{
		"storefile",
		required_argument,
		&storefile_opt,
		1
	},
	{
		"max",
		no_argument,
		&maxv,
		1
	},
	{
		"help",
		no_argument,
		&helpout,
		1
	},
	{}
};

char *main_helptxt =
"This program reads the power meter via a serial port for a period\n"
"of nseconds.  It will end the measurement upon receipt of the SIGUSR1\n"
"signal if nseconds is 0.  After the measurement is over, it outputs a\n"
"string indicating the total watts consumed during the measurement.";

char *opt_help[] = {
"	This option takes an argument which is the file pathname in which\n"
"	to store the readings from the power meter.  Readings are not stored\n"
"	without this option.  Must be a file name as stdout cannot be used.\n"
"	Readings are stored in a binary format which can be processed to ascii\n"
"	via the readings-dat2ascii program.",

"	In addition to total watts consumed, output the maximum value of each\n"
"	field over the entire measurement period.  An attempt is made to weed\n"
"	out values with a low confidence rating.",

"	Output this help message.",

	NULL,
};

 void
usage(int argn, char *estring)
{
	char *argstr;
	int x;

	if (argn > 0) {
		printf("%s: for option %s\n", estring, er_opts[argn].name);
	} else if (argn == 0) {
		/* if both arguments are 0, then usage can be used for basic help */
		if (estring) {
			printf("invalid option '%s'\n", estring);
		}
	}
	printf("\n");
	printf("usage: %s [<options>] <serial-port> <nseconds>\n\n", argvec[0]);
	printf(main_helptxt);
	printf("\n\n");
	printf("The valid options are:\n\n");
	for (x = 0; er_opts[x].name; x++) {
		switch (er_opts[x].has_arg) {
			case required_argument:
				argstr = "=<arg>";
				break;
			case optional_argument:
				argstr = "[=<arg>]";
				break;
			default:
				argstr = "";
				break;
		}
		printf("--%s%s\n", er_opts[x].name, argstr);
		printf(opt_help[x]);
		printf("\n");
	}
}

/*
 * see if a file is a device node
 */
 static int
is_dev(char *dname)
{
	struct stat st;

	stat(dname, &st);
	/*
	 * should we really allow block devices?  i guess so, i mean, whatever
	 */
	if (S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode) || S_ISFIFO(st.st_mode)
		|| S_ISSOCK(st.st_mode)) {
		return 1;
	}
	return 0;
}

 static int
is_rdev(char *dname)
{
	struct stat st;

	stat(dname, &st);

	if (S_ISCHR(st.st_mode)) {
		return 1;
	}
	return 0;
}


#define RS_NELEMENTS 9000
/*
 * pointer is used here in case we ever want to
 * dynamically allocate more arrays in addition to the static one.
 * or just do it all dynamically.
 */
struct reading *rsp;

struct reading readings_store[RS_NELEMENTS];

int rs_nelems;
extern int rs;  /* write index into the readings_store array */

struct timespec startclk;

int usr1sigrcv = 0;

 void
usr1handler(int signum)
{
	usr1sigrcv = 1;
}

sigset_t nullsst = {};
/*
 * struct sigaction usr1sigact = {
 *	sa_handler : usr1handler,
 *	sa_flags : SA_SIGINFO
 * };
 */
struct sigaction usr1sigact = {
	{usr1handler},
	sa_flags : SA_SIGINFO
};


 int
main(int argc, char **argv) {
	int rc;
	int storefile_fd;
	char storefile[1024];
	int argx;
	char *serialp;
	int mperiod;

	argvec = argv;

	/*
	 * process args
	 */
	do {
		rc = getopt_long(argc, argv, "", &er_opts[0], &argx);
		switch (rc) {
			case ':':
				usage(argx, "missing required argument");
				exit(1);
			case '?':
				usage(0, argv[optind - 1]);
				exit(1);
			case 0:
				/*
				 * check which option this is, and process option args if needed
				 */
				if (!strncmp(er_opts[argx].name, "storefile",
					strlen(er_opts[argx].name))) {

					strncpy(storefile, optarg, sizeof(storefile) - 1);
					if (!access(storefile, F_OK) && !is_dev(storefile)) {
						printf("'%s' already exists and is not a device/fd and"
								" that's a no-no\n", storefile);
						exit(1);
					}
				}
				/* else if {} */
				break;
		}
	} while (rc != -1);

	if (helpout) {
		usage(0, NULL);
		exit(0);
	}

	if ((argv[optind] == NULL) || (strlen(argv[optind]) == 0)) {
		printf("error: first argument must be serial port device file\n");
		usage(0, NULL);
		exit(1);
	}

	if ((argv[optind + 1] == NULL) || (strlen(argv[optind + 1]) == 0)) {
		printf("error: second argument must be measurement period in secs\n");
		usage(0, NULL);
		exit(1);
	}

	/*
	 * get the serial port
	 */
	serialp = argv[optind];
	if (!is_rdev(serialp)) {
		printf("error: '%s' not a character device\n", serialp);
		exit(1);
	}

	/*
	 * get the measurement period
	 */
	mperiod = (int)strtol(argv[optind + 1], NULL, 0);
	if ((mperiod > MAX_MPERIOD) || (mperiod < 0)) {
		printf("measurement period '%d' outside allowable range of 0 - %d seconds\n"
			"0 means measure until SIGUSR1 signal received (max %ds)\n",
			MAX_MPERIOD, mperiod, MAX_MPERIOD);
		exit(1);
	}
	if (mperiod == 0) {
		mperiod = MAX_MPERIOD; /* 1 hour */
		/*
		 * probably easier to just use siginterrupt(3) instead
		 */
		sigemptyset(&usr1sigact.sa_mask);
		rc = sigaction(SIGUSR1, &usr1sigact, NULL);
		if (rc == -1) {
			printf("Sigaction for USR1 signal failed.  errno = %d\n", errno);
			exit(1);
		}
	}

	/*
	 * initialize rsp for use by linked functions
	 */
	rsp = &readings_store[0];
	rs_nelems = RS_NELEMENTS;

	debugp("size of readings_store: %ld\n", sizeof(readings_store));
	// for RS_NELEMENTS=9000, this is 288000, or 281.25 KiB

	/*
	 * open the device and initialize the power meter
	 */
	rc = extech_power_meter(serialp);
	if (rc) {
		fprintf(stderr, "extech_power_meter returned errno '%d'\n", rc);
		exit(1);
	}
	printf("starting measurement process and sleeping for %ds...\n", mperiod);

	/* starts the measurement reading thread */
	start_measurement();

	/* sleep for the number of seconds the readings are to be collected */
	rc = sleep(mperiod);
	debugp("sleep returned %d, errno = %d", rc, errno);
	if ((rc > 0) && (errno == EINTR) && (usr1sigrcv)) {
		printf("sigusr1 rec'v after %d seconds\n", mperiod - rc);
	}

	/* reap the thread and clean up */
	end_measurement();

	printf("watt-hours consumed: %g\n", ex_joules_consumed());

	/*
	 * compute and print out the max values
	 */
	if (maxv) {
		struct reading m = {{0, 0}, 0, 0, 0, 0};
		int rx = 0;

		/*
		 * what is correct way to do this?  perhaps it is more correct
		 * to find the max watts, then take the other readings for that
		 * index as the other maxes.  that way, the amps, volts
		 * and pf would properly compute out to the watts number,
		 * whereas this way, they won't.
		 */
		for (rx = 0; rx < rs; rx++) {
			if (readings_store[rx].watts > m.watts) {
				m.watts = readings_store[rx].watts;
			}
			if (readings_store[rx].pf > m.pf) {
				m.pf = readings_store[rx].pf;
			}
			if (readings_store[rx].volts > m.volts) {
				m.volts = readings_store[rx].volts;
			}
			if (readings_store[rx].amps > m.amps) {
				m.amps = readings_store[rx].amps;
			}
		}
		/*
		 * TODO
		 * some kind of algorithm to sift out unlikely flyers from the
		 * wattage max
		 */
		printf("max: %7.3f watts %7.3f pf %7.3f volts %7.3f amps\n", m.watts,
			m.pf, m.volts, m.amps);
	}

	if (storefile_opt) {
		/*
		 * save the readings to a file, binary.  not tested
		 * trying to save to stdout, therefore that won't work.
		 */
		storefile_fd = open(storefile, O_CREAT | O_EXCL | O_RDWR, 0644);
		if (storefile_fd < 0 ) {
			fprintf(stderr,
			"open storefile '%s' failed.  open returned '%d' errno=%d\n",
			storefile, storefile_fd, errno);
		} else {
			rc = write(storefile_fd, &startclk, sizeof(struct timespec));
			rc = write(storefile_fd, &readings_store[0],
				sizeof(struct reading) * rs);
			close(storefile_fd);
			printf("saved %d readings to %s\n",
				rc / (int)sizeof(struct reading), storefile);
		}
	}
}

