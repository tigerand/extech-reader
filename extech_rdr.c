/*
 * Copyright 2017, Low Power Company, Inc.
 * Copyright 2017, Andrew Sharp
 *
 * Program to read the measured values from an extech power meter
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

 void *
usr1handler(int signum, siginfo_t *si, void *ucont)
{
	ucontext_t *ucontxt = ucont;

	usr1sigrcv = 1;
}

sigset_t nullsst = {};
/*
 * struct sigaction usr1sigact = {
 *	sa_sigaction : usr1handler,
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
	char storefile[256];
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
printf("optind=%d, optopt=%#x, argx=%d\n", optind, optopt, argx);
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
	if ((mperiod > 3600) || (mperiod < 0)) {
		printf("measurement period '%d' outside of allowable range (0 - 3600) seconds\n0 means measure until SIGUSR1 signal received (max 3600s)\n", mperiod);
		exit(1);
	}
	if (mperiod == 0) {
		mperiod = 3600;
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

	// printf("size of readings_store: %ld\n", sizeof(readings_store));
	// for RS_NELEMENTS=9000, this is 288000, or 281.25 KiB

	rc = extech_power_meter(serialp);
	if (rc) {
		fprintf(stderr, "extech_power_meter returned errno '%d'\n", rc);
		exit(1);
	}
	printf("starting measurement process and sleeping for %ds...\n", mperiod);
	start_measurement();
	rc = sleep(mperiod);
	debugp("sleep returned %d, errno = %d", rc, errno);
	if ((rc > 0) && (errno == EINTR) && (usr1sigrcv)) {
		printf("sigusr1 rec'v after %d seconds\n", mperiod - rc);
	}
	end_measurement();

	printf("watt-hours consumed: %g\n", ex_joules_consumed());

	/*
	 * print out the max values
	 */
	if (maxv) {
		struct reading m = {{0, 0}, 0, 0, 0, 0};
		int rx = 0;

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
		 * save the readings in a binary file
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

